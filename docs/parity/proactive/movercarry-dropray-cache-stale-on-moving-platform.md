# Mover platform-carry: stationary NPC reads a stale ground height on a vertically-moving mover (elevator floor-clip, #637)

**Confidence:** Medium-High for the root cause; **DEFERRED** for the fix (no surgical, build-verifiable change available).

## Original function + address (prose only)
In `Gothic2.exe`, a `zCMover` that is mid-motion advances every tick through
`zCMover::OnTick` (@0x00612f80) → `zCMover::AdvanceMover` (@0x00611d90) →
`zCMover::SetToKeyframe_KF` (@0x00611400) → `zCMover::InterpolateKeyframes_KF`
(@0x00611900). `InterpolateKeyframes_KF` repositions the mover vob each tick via
`zCVob::SetPositionWorld` (it is the only mutation in that routine besides
`SetRotationWorld`/`SetHeadingWorld`), which updates the mover's world-space
collision representation. The NPC/player movement step in the original re-runs its
downward ground trace against *current* world geometry every movement frame, so a
character standing still on the platform re-detects the platform's new height each
tick and tracks it up/down. There is no "skip the ground trace because the
character did not move" short-circuit keyed on the character's own position.

## OpenGothic file:line
- `game/game/movealgo.cpp:1047` `MoveAlgo::rayMain` (the land-ray cache)
- `game/game/movealgo.cpp:1061` `MoveAlgo::dropRay`
- `game/game/movealgo.cpp:13` `MoveAlgo::eps = 2.f`
- `game/game/movealgo.cpp:236` `ground = dropRay(pos, gValid)`
- `game/game/movealgo.cpp:250` early-out `if(dp==Vec3() && pos.y==ground && ...) return false;`
- `game/game/movealgo.h:135-144` `struct CacheLand : RayLandResult { float x,y,z; }; mutable CacheLand cache;`
- `game/world/triggers/movetrigger.cpp:150` `MoveTrigger::moveEvent` → `physic.setObjMatrix(transform())` (platform collision mesh is moved)

## Divergence
OpenGothic caches the result of the downward ground ray in `MoveAlgo::cache`,
keyed **only on the NPC's own query position** with a 2 cm epsilon
(`movealgo.cpp:1048`). The cache is initialized once (`cache.z = +inf`) and is
never reset per tick and never invalidated when world geometry under the NPC
changes — verified: the only writes to `cache.x/y/z` are at `movealgo.cpp:1055-1057`
inside the refresh branch; nothing in `movealgo.cpp/.h` or `npc.cpp` clears it.

Consequence for a character standing still on a moving mover (an elevator/lift
platform whose `cd_dynamic`/`cd_static` collision mesh is carried by
`MoveTrigger::moveEvent`):

- The NPC's position does not change (`dp==Vec3()`), so `rayMain` keeps returning
  the **stale** `cache.v.y` from before the platform moved.
- `dropRay` therefore reports the old ground height. With `pos.y==ground` (both
  stale and equal), the tick takes the early-out at `movealgo.cpp:250` and returns
  without adjusting the NPC.
- Platform rising: real floor climbs above the NPC's feet → the platform mesh
  passes *through* the standing character (the "elevator floor-clip", #637).
- Platform descending: the NPC is left floating at the old height instead of
  riding the platform down.

The original re-traces against the moved platform every frame and tracks it; the
position-keyed cache in OpenGothic suppresses that re-trace precisely in the
stand-still case where the support surface (not the NPC) is what moved. This is a
behavioral divergence in the platform-carry path, matching the #637/#623 reports.

## Proposed patch
**DEFERRED.** No surgical, high-confidence, build-verifiable fix is available
without a non-local change:

1. The correct match to the original is "re-trace ground every movement frame
   regardless of NPC displacement," i.e. remove/relax the position-keyed land-ray
   cache. But the cache is a deliberate performance optimization that every idle
   NPC relies on; dropping it for all NPCs is a broad behavioral/perf change, not a
   surgical edit. ("Empty beats false positives.")

2. A targeted fix — invalidate `cache` only when the supporting surface is a moving
   mover — cannot be expressed against existing grep-verified symbols:
   `DynamicWorld::RayLandResult` (`dynamicworld.h:146`) exposes only
   `mat`, `hasCol`, `sector`, and `Interactive* vob`; movers are `MoveTrigger`
   (collision via `PhysicMesh`), **not** `Interactive`, so the ray result carries
   no flag identifying that the ground hit belongs to an in-motion mover. Wiring
   that information through `DynamicWorld::ray`/`landRay` and the Bullet
   `btCollisionObject` user-pointer, then forcing a per-tick cache refresh while a
   mover under the NPC is ticking, is the right shape of fix but spans physics +
   movement + trigger subsystems and must be designed and verified as a unit.

Recommended follow-up (out of scope for a surgical patch): tag mover physic meshes
in the collision world, surface a "hit object is a live mover" bit on
`RayLandResult`, and have `MoveAlgo::rayMain` skip the position-keyed early-out (or
have `MoveTrigger::tick` request a ground-cache invalidation on riders) when that
bit is set, so a stationary rider re-traces and tracks the platform as in the
original.
