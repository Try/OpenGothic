# Mobsi use-distance is measured from the NPC bbox-center, not the world origin

**Confidence:** Medium-High (the npc-side metric divergence is decompiler-certain; the
practical magnitude depends on the ZS_POS node height and is partly entangled with the
already-documented `mv.y` flatten, which is why this is a refinement of `mobsi-use-distance.md`
rather than a contradiction of it).

## Original function + address

`oCMobInter::SearchFreePosition(oCNpc*, float maxDist)` at `Gothic2.exe 0x0071dfc0`
(reached from `oCMobInter::CanInteractWith` `0x720f40` and `oCMobInter::GetFreePosition`
`0x71df50`, both passing `maxDist = 150.0`).

For every free `ZS_POS` slot the engine computes a **3D squared distance** between two points
and rejects the slot when `maxDist*maxDist < squaredDistance`:

- The NPC point is the NPC's **world origin** — the vob trafo translation read from fields
  `npc+0x48` (X), `npc+0x58` (Y), `npc+0x68` (Z). That origin sits at the NPC's feet; it is
  the same value `oCMobInter::SetIdealPosition` later feeds back into `SetPositionWorld` to
  preserve the NPC's current world position.
- The slot point is the **cached `ZS_POS` node world position** (`TMobOptPos+0x0c/+0x1c/+0x2c`),
  captured once in `oCMobInter::ScanIdealPositions` `0x71dc30` via
  `GetTrafoModelNodeToWorld` — i.e. the real authored node height, never flattened.

So vanilla measures `dist(npcFeet, realNodePos)` in full 3D and accepts the slot only inside
150 units.

## OpenGothic file:line

`game/world/objects/interactive.cpp:853` (in `Interactive::attach(Npc&, Pos&)`):

```cpp
auto mat = nodeTranform(&npc,to);
Tempest::Vec3 mv = {};
mat.project(mv);                       // mv = real slot world position
if(!to.isDistPos())
  mv.y = npc.position().y;             // flatten slot Y to npc feet (for setPos, documented)
...
static const float MOBSI_USE_DISTANCE = 150.f;
if((npc.centerPosition()-mv).quadLength()>MOBSI_USE_DISTANCE*MOBSI_USE_DISTANCE) {
```

`Npc::position()` returns the vob origin `{x,y,z}` (npc.cpp:`Vec3 Npc::position() const`), which
is exactly the original's `+0x48/+0x58/+0x68`. `Npc::centerPosition()` returns
`position() + pose().translateY()` (npc.cpp:711), adding the model's vertical half-height
(~90 cm) — a quantity with **no analog** in `SearchFreePosition`.

## Divergence

Two deviations from the vanilla metric both push the same direction:

1. **NPC point:** OpenGothic uses `npc.centerPosition()` (origin + `pose.translateY()`), the
   original uses the bare origin (`npc.position()`).
2. **Slot point:** for non-DIST slots OpenGothic reuses `mv` whose Y was just flattened to the
   NPC's feet, whereas the original keeps the real node Y.

For a normal seat/chest slot the combined effect is a near-constant `~pose.translateY()`
(~90 cm) vertical term baked into the squared distance: `(center - feetFlattenedSlot).y` is
exactly `pose.translateY()`. At the 150 boundary that converts an authored horizontal reach of
150 into an effective horizontal reach of only `sqrt(150^2 - 90^2) ≈ 120`. The player therefore
gets the "too far away" rejection (and NPCs abort their approach) ~20% closer than vanilla,
*on top of* the 165→150 constant already corrected in `mobsi-use-distance.md`. The faithful
metric is "npc origin → real ZS_POS node position", which the patch below restores.

## Proposed patch

File: `game/world/objects/interactive.cpp`

OLD:
```cpp
  Tempest::Vec3 mv = {};
  mat.project(mv);

  // NOTE: in original-game oCMobInter::SetIdealPosition (Gothic2.exe 0x71e240), the
  // non-DIST slot branch does SetPositionWorld(slot.x, npc.currentY, slot.z): X/Z come
  // from the ZS_POS node, but the NPC keeps its own world Y. ZS_POS nodes are authored
  // at seat/model height, so taking the node Y directly floats or sinks the NPC (#647).
  if(!to.isDistPos())
    mv.y = npc.position().y;

  // NOTE: in original-game oCMobInter::SearchFreePosition (Gothic2.exe 0x71dfc0), called
  // from CanInteractWith/GetFreePosition, uses a free-slot search distance of 150.0 (a slot
  // is rejected once npc->slot exceeds 150) -- not MAX_AI_USE_DISTANCE (165). Using 165 let
  // NPCs/the player start using mobsis ~10% farther than vanilla.
  static const float MOBSI_USE_DISTANCE = 150.f;
  if((npc.centerPosition()-mv).quadLength()>MOBSI_USE_DISTANCE*MOBSI_USE_DISTANCE) {
```

NEW:
```cpp
  Tempest::Vec3 mv = {};
  mat.project(mv);
  const Tempest::Vec3 slotPos = mv;   // real ZS_POS node world position (pre Y-flatten)

  // NOTE: in original-game oCMobInter::SetIdealPosition (Gothic2.exe 0x71e240), the
  // non-DIST slot branch does SetPositionWorld(slot.x, npc.currentY, slot.z): X/Z come
  // from the ZS_POS node, but the NPC keeps its own world Y. ZS_POS nodes are authored
  // at seat/model height, so taking the node Y directly floats or sinks the NPC (#647).
  if(!to.isDistPos())
    mv.y = npc.position().y;

  // NOTE: in original-game oCMobInter::SearchFreePosition (Gothic2.exe 0x71dfc0) the free-slot
  // reject distance is a full 3D squared distance from the NPC's *world origin* (vob trafo
  // fields +0x48/+0x58/+0x68 == position(), the feet) to the *cached ZS_POS node world position*
  // (+0x0c/+0x1c/+0x2c) -- not npc.centerPosition() and not the Y-flattened setPos target.
  // Using centerPosition() (origin + pose.translateY()) against the feet-flattened slot baked a
  // spurious ~90cm vertical term into the check, shrinking the player's effective reach below 150.
  static const float MOBSI_USE_DISTANCE = 150.f;
  if((npc.position()-slotPos).quadLength()>MOBSI_USE_DISTANCE*MOBSI_USE_DISTANCE) {
```

(`slotPos` keeps the un-flattened node Y for the distance test only; `mv` is still flattened for
the subsequent `setPos(npc,mv)`, preserving the documented #647 behaviour. For DIST slots the
original tests against the raw node rather than OpenGothic's direction-projected point, a minor
edge case left unchanged.)
