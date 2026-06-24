# Steering: obstacle side-step turn rate hardcoded to 360 deg/s instead of guild turn_speed

**Confidence:** Medium-High

## Original function + address (prose only)

In `Gothic2.exe` the NPC obstacle-avoidance / collision side-step is the "robust
trace" (Rbt) subsystem in `oNpc_Move.cpp`. The functions that actually rotate the
NPC away from an obstacle while it is jammed against geometry — `oCNpc::RbtAvoidObstacles`
(@0x006875e0) and the general goto turn `oCNpc::Turning` (@0x00683120) — both derive
their per-frame turn rate from `oCNpc::GetTurnSpeed` (@0x00680970).

`GetTurnSpeed` returns the NPC's configured turn speed (the float stored at object
offset `+0x49c`, which is seeded from the guild `turn_speed` value) UNLESS the NPC is
currently registered as colliding (the virtual "is-colliding" predicate at vtbl `+0x100`
returns non-zero), in which case it returns the literal constant `0x3DCCCCCD` = **0.1
deg per millisecond = 100 deg/s**. The per-frame turn applied in `RbtAvoidObstacles`
and `Turning` is `GetTurnSpeed() * frameDeltaMs`. Because an NPC running obstacle
avoidance is, by definition, jammed against the obstacle (colliding), the effective
avoidance turn rate in the original is capped at **0.1 deg/ms (100 deg/s)**, and when
not flagged colliding it is the guild `turn_speed` (stock Gothic II humans ≈ 90 deg/s).

## OpenGothic file:line

`game/game/movealgo.cpp:915` (and its use at lines 921, 970, 973) inside
`MoveAlgo::onMoveFailed`.

## Divergence

OpenGothic's `onMoveFailed` is the side-step/wall-slide reaction to a blocked move. It
hardcodes the avoidance turn rate:

```cpp
static float speed = 360.f;
...
const float stp = speed*float(dt)/1000.f;   // line 921
...
if(val<-threshold) setDirection(npc.rotation()-stp);   // line 970
else if(val>threshold) setDirection(npc.rotation()+stp);// line 973
```

So OpenGothic turns blocked NPCs at a fixed **360 deg/s**, independent of guild
`turn_speed`, while the original turns at the guild `turn_speed` (~90 deg/s) capped at
**100 deg/s** while colliding. That is roughly a **3.6×–4× too-fast** avoidance turn:
NPCs whip around obstacles / along walls far quicker than in the original, and the rate
ignores per-guild tuning entirely. (OpenGothic's own normal turning, `Npc::rotateTo` /
`implTurnTo`, already correctly uses `guildVal().turn_speed[gl] * dt/1000` — i.e. the
same `turn_speed`-deg/s convention — so only the avoidance path diverges.)

Note: the broader Rbt waynet/raytrace avoidance architecture is NOT reimplemented in
OpenGothic (`onMoveFailed` is an explicit "emulate bouncing behaviour" approximation),
so this finding is limited strictly to the turn-rate scalar, which IS a directly
comparable, isolated numeric value.

## Proposed patch

Replace the hardcoded 360 deg/s with the NPC's guild `turn_speed` (the same value the
original `GetTurnSpeed` returns and the same value OpenGothic's own `rotateTo` uses).
This keeps `onMoveFailed`'s structure intact and only fixes the scalar. Symbols verified
to exist and to be used elsewhere in this same file: `npc.guild()` and
`npc.world().script().guildVal().<field>[gl]` (see `slideAngle`/`waterDepthKnee` at
movealgo.cpp:614-640).

OLD (`game/game/movealgo.cpp:913-921`):
```cpp
void MoveAlgo::onMoveFailed(const Tempest::Vec3& dp, const DynamicWorld::CollisionTest& info, uint64_t dt) {
  static float threshold = 0.4f;
  static float speed     = 360.f;

  if(dp==Tempest::Vec3())
    return;

  const auto  ortho   = Tempest::Vec3::crossProduct(Tempest::Vec3::normalize(dp),Tempest::Vec3(0,1,0));
  const float stp     = speed*float(dt)/1000.f;
```

NEW:
```cpp
void MoveAlgo::onMoveFailed(const Tempest::Vec3& dp, const DynamicWorld::CollisionTest& info, uint64_t dt) {
  static float threshold = 0.4f;

  if(dp==Tempest::Vec3())
    return;

  // NOTE: in original-game oCNpc::RbtAvoidObstacles @0x006875e0 / oCNpc::Turning @0x00683120
  // the avoidance turn rate is oCNpc::GetTurnSpeed() @0x00680970 * frameDeltaMs, i.e. the
  // guild turn_speed (deg/s), capped at 0.1 deg/ms (=100 deg/s) while the NPC is colliding.
  // The previous hardcoded 360 deg/s spun blocked NPCs ~3.6x too fast and ignored guild tuning.
  const auto  gl      = npc.guild();
  const float speed   = std::min(float(npc.world().script().guildVal().turn_speed[gl]), 100.f);
  const auto  ortho   = Tempest::Vec3::crossProduct(Tempest::Vec3::normalize(dp),Tempest::Vec3(0,1,0));
  const float stp     = speed*float(dt)/1000.f;
```

If the maintainers prefer not to apply the colliding-state `min(...,100)` cap (the NPC
in `onMoveFailed` is by construction blocked, so the original's collide branch applies),
the conservative variant is to drop the `std::min` and use the raw guild `turn_speed`,
which still removes the 360 hardcode and restores guild scaling.

**Build note:** `<algorithm>` is required for `std::min`; it is already pulled in
transitively (used as `std::min`/`std::max` at movealgo.cpp:247,596, etc.), so no new
include is needed.
