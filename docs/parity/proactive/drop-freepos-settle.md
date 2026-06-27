# Dropped item is placed without a free-position / collision-settle search

**Confidence:** Medium (divergence is certain and verified; player-visible impact is partly masked by Bullet physics, and a parity-faithful fix is large → patch DEFERRED)

## Original function + address (prose only)

When an NPC drops a vob, `oCNpc::DoDropVob` (Gothic2.exe `0x00744dd0`) computes a
single desired drop position (the hand/model-node world translation) and then hands
the vob to `oCAIVobMove::Init` (`0x0069f540`), passing that desired position, the NPC
vob, and the NPC world trafo.

Inside `oCAIVobMove::Init`, **before** the item is finally positioned, the engine calls
`oCVob::SearchFreePosition` (`0x0077ccb0`). That routine does not blindly place the item
at the desired point. It:

- derives a step distance from the item's own bounding-box half-extents (clamped to a
  minimum of 10 units on X and Z);
- walks a small grid of candidate positions offset along the dropping NPC's forward
  ("At") vector (`zMAT4::GetAtVector` of the NPC trafo) — roughly 8 forward bands times
  a handful of lateral steps;
- for each candidate, ray-traces from the NPC origin to the candidate
  (`zCWorld::TraceRayNearestHit`) to confirm the spot is *reachable* (not behind a wall),
  and then calls `oCVob::HasEnoughSpace` (`0x0077c6b0`), which temporarily moves the
  item's trafo to the candidate and runs `zCVob::DetectCollision` to confirm the spot is
  *empty*;
- returns the first reachable, collision-free candidate, or — failing all of them — warns
  `"U: VOBAI: Not enough space"` and falls back to the original desired position.

Only after this search does `Init` call `ResetRotationsWorld` + `SetPositionWorld` and
enable physics. So in the original game a dropped item is nudged to the nearest reachable,
non-overlapping spot in front of the NPC; it does not start its physics life embedded in
a wall, inside another vob, or on the far side of geometry from the NPC.

## OpenGothic file:line

- `game/world/objects/npc.cpp:3700-3704` (`Npc::dropItem`) — builds `drop` purely from the
  hand-bone world translation and calls `owner.addItemDyn(id,drop,...)`.
- `game/world/worldobjects.cpp:682-701` (`WorldObjects::addItemDyn`) — creates the dynamic
  item, enables physics, and does `it->setObjMatrix(pos)` with the raw position. No free-
  position / accessibility / overlap search exists for dropped items anywhere in the drop
  path (grep for `SearchFreePosition` / `HasEnoughSpace` over `game/` finds only the
  unrelated `oCMobInter` reference in `interactive.cpp:848`).
- Same gap applies to the weapon/shield drop path
  (`game/graphics/mdlvisual.cpp:293,314` → `addItemDyn`).

## Divergence

OpenGothic places the dropped item at exactly the hand position and relies entirely on
the Bullet rigid body to resolve any overlap and fall to rest. The original first runs a
deterministic search for a reachable, collision-free position in front of the NPC and
places the item there. Observable consequences where Bullet does not fully mask it:

- dropping while facing/standing very close to a wall or large vob: original slides the
  item to a reachable open spot; OpenGothic spawns it in the overlap and lets physics eject
  it in a non-deterministic direction (can end up clipped, on the wrong side of thin
  geometry, or briefly tunneling).
- the deterministic forward-offset placement means original drops land slightly *in front*
  of the NPC along its facing; OpenGothic drops land straight down at the hand, so a moving
  NPC's drop lands relatively further back.

## Proposed patch

DEFERRED.

Reasons:
1. Faithful reproduction requires re-implementing the full `SearchFreePosition` grid
   (step distance from item bbox half-extents, the exact forward-band / lateral ordering,
   the NPC-origin→candidate reachability ray, and the `HasEnoughSpace`/`DetectCollision`
   empty-space test). That is a substantial, multi-function port, not a surgical edit, and
   getting the grid ordering / step constants wrong would itself be a parity divergence.
2. OpenGothic already runs a real Bullet dynamic body for dropped items
   (`DynamicWorld::dynamicObj`, `game/physics/dynamicworld.cpp:848`), so overlaps are
   resolved by physics rather than left embedded. This partially masks the divergence and
   makes a half-accurate hand-rolled search a net regression risk ("empty beats false
   positives").

A correct future fix would add, in `WorldObjects::addItemDyn` (or a helper called from the
drop paths in `Npc::dropItem` and `MdlVisual::dropWeapon/dropShield`), a free-position
search using the existing physics ray API (`World::physic()->...`) and the item's
`bBox()` to mimic `oCVob::SearchFreePosition`, with a
`// NOTE: in original-game oCVob::SearchFreePosition @0x0077ccb0 (from oCAIVobMove::Init
@0x0069f540, oCNpc::DoDropVob @0x00744dd0) ...` citation.

### Secondary, same-function observation (also DEFERRED)

`oCAIVobMove::Init` additionally gives the dropped body a non-zero **initial velocity**
(`zCRigidBody::SetVelocity`, derived from the NPC trafo with a 100.0 scale argument passed
from `DoDropVob`), i.e. the item is tossed a short distance rather than released at rest.
OpenGothic spawns the body with zero initial velocity. This is a real divergence (the
"throw arc") but the exact velocity vector cannot be reconstructed from the decompiler with
parity confidence (the float arguments to `Alg_Rotation3DN` / the 100.0 scale are not
recoverable cleanly), so any concrete value would be a guess → DEFERRED.
