# Wld_InsertNpc rotates the inserted NPC to the waypoint heading; the original places it position-only

**Confidence:** Medium (the original-side placement primitive is decompile-verified to be
position-only — same pattern as the confirmed `Wld_InsertItem` case — but the long-term
observable difference is muddied by AI/routine realignment, so the patch is DEFER-leaning,
mirroring the already-deferred `itemplace-waypoint-direction`.)

## Original function + address (prose)

`Wld_InsertNpc(npc, "SPAWNPOINT")` is handled by `FUN_006df1f0` (the `Wld_InsertNpc`
external in `P:\dev\g2addon\release\Gothic\_ulf\oGameExternal.cpp`). It creates the
NPC via the object factory and forwards to `oCSpawnManager::SpawnNpc(int,zSTRING&,float)`
(`0x00778b20`) → `oCSpawnManager::SpawnNpc(oCNpc*,zSTRING&,float)` (`0x00778ba0`), which
resolves the spawnpoint name to a `zVEC3` **position only**:

1. exact waypoint via `zCWayNet::GetWaypoint` (`0x007b0330`) → `zCWaypoint::GetPositionWorld`,
2. else exact vob via `oCWorld::SearchVobByName` (`0x00780610`) → its trafo translation.

The NPC is then placed (when within insert range) by `oCSpawnManager::InsertNpc`
(`0x00778920`), whose only placement call is the virtual `npc->vtable[0xCC](position)` —
`zCVob::SetPositionWorld` (`0x0061bb70`). `SetPositionWorld` writes **only** the
translation column of the trafo (offsets 0x50/0x60/0x70) and leaves the rotation
sub-matrix untouched, i.e. the inserted NPC keeps its construction rotation (identity).
At no point is the spawnpoint/waypoint *heading* applied. This is the exact same
primitive used by `Wld_InsertItem` (`FUN_006e0520`), where I confirmed the placement is
`zCVob::SetPositionWorld(item, position)` — position only — so the engine pattern is
"insert at waypoint position, do not adopt waypoint heading."

## OpenGothic file:line

`game/world/worldobjects.cpp:297` (in `WorldObjects::addNpc(size_t, std::string_view at)`,
the `Wld_InsertNpc` path via `GameScript::wld_insertnpc` → `World::addNpc`):

```cpp
npc->setPosition  (pos->position() );
npc->setDirection (pos->direction());   // <-- adopts waypoint heading
npc->attachToPoint(pos);
```

`Npc::setDirection(const Vec3&)` (`game/world/objects/npc.cpp:460`) is a real rotation
(`angle = angleDir(...)`, `physic.setRotation(angle)`), not a no-op like `Item::setDirection`.
So OpenGothic immediately turns the freshly inserted NPC to face `pos->direction()` (the
waypoint/freepoint heading).

## Divergence

On `Wld_InsertNpc` the original leaves the inserted NPC at its identity rotation
(`SetPositionWorld` is position-only); OpenGothic rotates it to the spawnpoint's heading.
For NPCs that are subsequently realigned by their daily-routine ZS state this converges,
so the difference is (a) the immediate post-insert facing of every inserted NPC and
(b) the *persistent* facing of routine-less NPCs (e.g. inserted monsters), which keep
identity rotation in the original but face the waypoint/freepoint direction in OpenGothic.

This is the NPC analogue of the item case: for items OpenGothic's `Item::setDirection`
no-op accidentally matches the original (identity), whereas for NPCs `Npc::setDirection`
is functional and therefore diverges.

## Proposed patch (DEFER-leaning)

`game/world/worldobjects.cpp:297`

```cpp
// OLD
    npc->setPosition  (pos->position() );
    npc->setDirection (pos->direction());
    npc->attachToPoint(pos);
// NEW
    // NOTE: in original-game the Wld_InsertNpc placement (oCSpawnManager::InsertNpc
    // @0x00778920 -> zCVob::SetPositionWorld @0x0061bb70, same primitive as
    // Wld_InsertItem FUN_006e0520) writes only the trafo translation and preserves the
    // NPC's identity rotation -- the spawnpoint heading is NOT adopted at insert time
    // (the daily-routine ZS state is what later orients routine NPCs). OpenGothic's
    // setDirection() instead turned the NPC to face the waypoint immediately, diverging
    // for routine-less NPCs (monsters keep identity rotation in the original).
    npc->setPosition  (pos->position());
    npc->attachToPoint(pos);
```

DEFER rationale: identical reasoning to `itemplace-waypoint-direction` — the change is
visual-only (NPC yaw at spawn), it interacts with the routine realignment path, and a
wrong call would mis-orient world-start NPC batches, so it wants an in-game visual check
before applying. The `attachToPoint` / `currentWayPoint` bookkeeping and the position are
unchanged; only the heading adoption is dropped to match the original primitive.
