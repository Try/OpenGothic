# Flee / Give-Up-Chase Engine Parity — NO FINDING

**Confidence:** High (that there is no surgical, non-excluded engine divergence)

## Scope investigated

Engine-side NPC flee / give-up-chase logic in the original `Gothic2.exe` versus
OpenGothic.

### Original engine functions examined
- `oCNpc::Fleeing` @ `0x006820c0` — the flee-steering routine.
- `oCNpc::ThinkNextFleeAction` @ `0x006820d0` — byte-for-byte identical body to
  `Fleeing` (same `oNpc_Move.cpp` source); same logic.
- `oCNpc::AI_Flee` @ `0x00683210` — **empty stub** in retail (`return;`), so flee
  is not triggered or gated in the engine; it is driven entirely from Daedalus
  (`ZS_MM_Flee` / `B_*`) via AI messages that invoke the steering routine.

### OpenGothic counterparts examined
- `Npc::implAiFlee` — `game/world/objects/npc.cpp:2045`
- `Npc::GoTo::setFlee` / `GT_Flee` handling — `game/world/objects/npc.cpp:94`, `:1591`, `:2069`, `:2624`
- AI dispatch `case AI_Flee` — `game/world/objects/npc.cpp:2974`

## Why NO FINDING

Decomposing the original `oCNpc::Fleeing` shows its entire body reduces to three
pieces, and each is either faithful, already-deferred, or purely architectural:

1. **Free-line-of-sight gate** at the top of `Fleeing` (`FreeLineOfSight(this,target)`;
   returns without steering when the enemy is not visible). This is the
   already-deferred **flee-los-gate** item — excluded.

2. **Flee-direction / away-vector.** Original computes `away = (enemyPos - selfPos) * -2.0`
   (the constant `___real_c0000000` = -2.0f), i.e. it always steers along the
   `self - enemy` direction. OpenGothic's `implAiFlee` does the same: `dx = oth.x - x`,
   `dz = oth.z - z`, then `implTurnTo(-dx, -dz, ...)` (npc.cpp:2075-2077). The flee
   direction is faithful — no divergence.

3. **Waypoint sampling/selection along the away direction** (the loop sampling up to
   4 nearby waypoints at `self + away`, halving the offset with the `1/n` and `-1.0`
   = `___real_bf800000` scaling, picking a waypoint distinct from the previous one
   `this+0x4c0`, then dispatching an `oCMsgMovement(3, wpName)`), plus the
   `RobustTrace` / `RbtUpdate` message-driven movement and the stuck-detection state
   machine (`this+0x4a0` flee-active, `this+0x4a4` stuck flag, comparing the stored
   flee target `this+0x4b4..0x4bc` against the current translation). The
   waypoint-selection part is the already-deferred
   **flee-waypoint-selection-distance-adaptive** item — excluded. The
   RobustTrace/message-queue movement and the stuck/cornered re-pick are an
   *architectural* difference: OpenGothic re-evaluates the flee waypoint and turn
   every tick through `MoveAlgo` + `implTurnTo`, rather than emitting a one-shot
   "go-to-waypoint" movement message and polling for being stuck. This is not a
   surgical, build-verifiable one-liner.

### Other candidates ruled out
- **Flee-trigger HP threshold:** script-side (`B_AssessDamage` / `ZS_MM_Flee`); the
  engine `AI_Flee` is an empty stub, so there is no engine threshold to diverge.
- **Give-up-chase distance/time:** the attacker-side "enemy too far, abandon chase"
  decision is script-side in both (`ZS_MM_Attack`); no engine distance/time gate
  exists in `Fleeing` or a dedicated engine routine. OpenGothic likewise has no
  engine chase-distance gate (target is cleared via `setTarget(nullptr)` from
  higher-level/script paths).
- **Resume-combat-after-flee / flee movement speed:** handled by the script state
  machine and the shared body-state/run flags; no engine-specific flee divergence
  identified.

## Conclusion

The only engine-side flee logic (`oCNpc::Fleeing` / `ThinkNextFleeAction`) maps onto
the two already-deferred items (flee-los-gate, flee-waypoint-selection-distance-adaptive)
plus an architectural movement-driver difference. The flee direction (away-vector) is
faithful. No additional surgical, high-confidence, build-verifiable divergence found.

**NO FINDING.**
