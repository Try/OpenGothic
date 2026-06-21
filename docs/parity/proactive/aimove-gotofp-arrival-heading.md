# AI_GotoFP arrival completion ignores the freepoint's facing direction

**Confidence:** Medium

## Original function + address
`oCNpc::EV_GotoFP` @ **0x685700** (`oNpc_Move.cpp`), the handler invoked for movement
sub-type `0xe` (the message posted by the `AI_GotoFP` external, whose handler is at
**0x6ee3a0** in `oCGame::DefineExternals_Ulfi` and builds an `oCMsgMovement` of sub-type
`0xe`). The movement dispatch table inside `oCNpc::OnMessage` @ **0x74b020** routes
sub-type `0xe -> EV_GotoFP`.

Behaviour, described in prose (no decompiled source reproduced):

- On the first tick `EV_GotoFP` resolves the target free-point/spot via
  `FindSpot(name, …, 700.0)`, seeds the robust-trace arrival radius² field
  (`this+0x4e4`) to `0x451c4000` = **2500.0** (i.e. a 50-unit arrival radius), runs
  `RbtReset`/`RbtUpdate`, and marks the spot as used.
- Each subsequent tick it calls `RobustTrace`. *Only after* `RobustTrace` reports the NPC
  has reached the spot does the function enter its completion tail.
- In that tail it builds a heading target = `spot_position + spot_direction * 200.0`
  (`0x43480000` = 200.0 applied to the spot's local forward vector taken from the spot
  vob matrix), calls the turn helper `FUN_00683000` (the `ABS`-returning variant of
  `oCNpc::Turning` @ 0x683120; it issues `oCAniCtrl_Human::TurnDegrees` toward the target
  and returns the remaining absolute angle in **degrees**), and returns
  `(int(|remaining_angle|) < 3)` — i.e. the `AI_GotoFP` action only reports **complete**
  once the NPC is both within 50 units **and** turned to within ~3° of the free-point's
  own facing direction. Until then the action stays queued and the NPC keeps rotating in
  place.

So in the original, "arriving" at a free-point via `AI_GotoFP` inherently re-orients the
NPC to the free-point's stored heading as part of completion.

## OpenGothic file:line
`game/world/objects/npc.cpp:1531-1560` (`Npc::implGoTo`, the `go2.isClose(...)` /
`if(finished)` arrival block), reached for `AI_GotoFP` via
`GameScript::ai_gotofp` (`game/game/gamescript.cpp:3099`) ->
`AiQueue::aiGoToPoint` -> `AI_GoToPoint` (`npc.cpp:2535`) -> `go2.set(...)` with the
default hint `GT_Way`.

## Divergence
For a plain free-point goto OpenGothic sets the goto hint to `GT_Way`. In the arrival
`if(finished)` block it performs the heading turn **only** when
`go2.flag==Npc::GT_NextFp` (`npc.cpp:1545`); for `GT_Way` it snaps X/Z onto the target
(the #585 fix) and immediately `clearGoTo()`s **without any rotation**. The NPC therefore
stops facing whatever direction it was travelling in, rather than being turned to the
free-point's stored `dir` the way `EV_GotoFP` does (turn-to-spot-heading, finish within
~3°). This is user-visible when `AI_GotoFP` is used for ambient positioning without a
following `AI_AlignToFP` (common in `ZS_*` daily routines): in the original the NPC ends
up oriented to the free-point; in OpenGothic it ends up oriented along its approach path.

This is **distinct from issue-585**: that finding/fix concerns the arrival *position*
re-snap (`RbtMoveToExactPosition`, applied) and explicitly *deferred* the `EV_AlignToFP`
position re-snap. Neither addresses the missing on-arrival *heading* for `AI_GotoFP`
itself. It is also not a turn-speed nor a waynet-edge-cost issue.

## Proposed patch
Mirror `EV_GotoFP`'s completion: when a `GT_Way` goto finishes onto a free-point that
carries a facing direction, turn toward that direction (using the existing standard turn,
which already stops within an angular threshold) before the NPC is released from the goto,
exactly as the original keeps the action queued until aligned. Grep-verified symbols:
`Npc::currentFp` (npc.h:629), `WayPoint::isFreePoint()` (waypoint.h:22),
`WayPoint::dir` (waypoint.h:32), `Npc::implTurnTo(const WayPoint*, AnimationSolver::TurnType, uint64_t)`
(npc.cpp:1485), `AnimationSolver::TurnType::Std`.

OLD (`game/world/objects/npc.cpp`, arrival block):
```cpp
    if(finished) {
      if(go2.flag==Npc::GT_NextFp && implTurnTo(go2.wp,AnimationSolver::TurnType::Std,dt))
        return true;
```
NEW:
```cpp
    if(finished) {
      // NOTE: in original-game oCNpc::EV_GotoFP @0x685700 (oNpc_Move.cpp), arrival is not
      // complete until the NPC has also turned to within ~3deg of the free-point's stored
      // facing direction (target = spotPos + spotDir*200, oCNpc::Turning @0x683120). A plain
      // GT_Way free-point goto must re-orient to currentFp->dir before being released, the
      // same way GT_NextFp already does.
      if(go2.flag==Npc::GT_NextFp && implTurnTo(go2.wp,AnimationSolver::TurnType::Std,dt))
        return true;
      if(go2.flag==Npc::GT_Way && currentFp!=nullptr && currentFp->isFreePoint() &&
         (currentFp->dir.x!=0.f || currentFp->dir.z!=0.f) &&
         implTurnTo(currentFp->dir.x,currentFp->dir.z,AnimationSolver::TurnType::Std,dt))
        return true;
```

DEFERRED-alternative note: if there is concern that some `GT_Way` free-point arrivals are
*not* `AI_GotoFP` (e.g. a free-point that happens to terminate a way-route), the turn could
be gated more tightly, but every `GT_Way` whose final attached point `isFreePoint()` and has
a non-zero `dir` matches the `EV_GotoFP` semantics, so the gate above is already conservative
(waypoints have `freePoint==false` and are unaffected). Confidence is Medium rather than High
only because the original couples this turn into `EV_GotoFP` while OpenGothic splits goto and
align, so masking by a following `AI_AlignToFP` can hide the difference in some routines.
