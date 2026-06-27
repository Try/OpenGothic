# ZS_ state Loop re-invoke interval is throttled to perceptionTime (should run every AI tick)

**Confidence:** Medium-High on the divergence (factually solid); **fix = DEFERRED** (non-surgical, high blast-radius).

## Original function + address (prose only)

In `Gothic2.exe` the per-NPC AI update `oCAIHuman::DoAI` (@0x0069bab0) is invoked once per AI
frame for every in-range NPC. It unconditionally calls `oCNpc_States::DoAIState`
(@0x0076d1a0) at the end of that update — there is no per-NPC time gate in front of the call
other than the global debug knob `ai_scriptStateSkip` (default 0, i.e. disabled; see
`oCNpc::IsScriptStateAIEnabled` @0x0075f3e0 and `oCNpc::ProcessAITimer` @0x0075f360).

Inside `DoAIState`, the internal phase lives in the `oCNpc_States` object: phase -1 = init
(calls funcIni, then `SetPerceptionTime(5000)`, advances to phase 0), phase 0 = loop, phase 1 =
end (calls funcEnd, deactivates), phase 2 = done. In the phase-0 branch the loop function is
called on **every** `DoAIState` invocation with no internal interval:

- if the state has no loop function the return is forced to 1 (LOOP_END);
- otherwise the loop function is called via `zCParser::CallFunc` and its return value is read;
- a non-zero return advances the phase to 1 (end), zero keeps phase 0.

So the loop function (`ZS_xxx_Loop`) re-runs **every AI frame** (~framerate cadence) for nearby
NPCs. Perception *assessment* is throttled independently by the NPC's own perception timer
(`oCNpc` fields at 0x900/0x904, written by `oCNpc::SetPerceptionTime` @0x0075dba0; the auto
`oCNpc::PerceiveAll` @0x0075dbe0 path is gated by that timer, not by the state loop). The two
cadences are decoupled in the original: **state loop = per frame, perception = per perceptionTime.**

## OpenGothic file:line

`game/world/objects/npc.cpp:3178-3179` (`Npc::tickRoutine`):

```
if(aiState.loopNextTime<=owner.tickCount()) {
  aiState.loopNextTime = owner.tickCount() + perceptionTimeClampt();
  ...
  loop = sc.invokeState(this,currentOther,currentVictim,aiState.funcLoop);
```

`perceptionTimeClampt()` (npc.cpp:4426) returns `max(perceptionTime,1)`, and `perceptionTime`
defaults to 5000ms (npc.h:579), commonly lowered to ~1000ms by `Npc_SetPercTime`. The same
quantity also schedules the genuine perception scan via `perceptionNextTime`
(npc.cpp:4458/4493, consumed in `worldobjects.cpp:251`).

## Divergence

OpenGothic re-invokes the ZS state loop function at the **perceptionTime** interval
(`loopNextTime += perceptionTimeClampt()`), i.e. once every ~1-5 seconds, whereas the original
runs it **every AI frame**. OpenGothic conflates the perception-scan cadence with the state-loop
cadence; the original keeps them separate.

OpenGothic partially papers over this by force-poking `aiState.loopNextTime = owner.tickCount()`
on specific combat events (npc.cpp:1832, 1900) and via `implFaiWait` (npc.cpp:1958), and by the
`fastPath` skip (3183). Those patches exist precisely because the base cadence is throttled.
Non-combat states (routine micro-decisions, assessment-driven `ZS_*` loops, talk/idle
transitions that poll conditions each frame in the original) therefore react up to one
perceptionTime later than in `Gothic2.exe`.

This is distinct from the already-documented perception items (`aistate-perctime-default.md`,
`aistate-perctime-reset-on-state-init.md`): those concern the perception *default/reset value*;
this concerns the *state-loop re-invoke cadence* being bound to that value at all.

## Proposed patch

**DEFERRED.**

Reason: the parity-correct change is to decouple the ZS loop cadence from `perceptionTime` and
re-invoke `aiState.funcLoop` every AI tick (the original calls the loop every `DoAIState`/frame,
while leaving the existing separate `perceptionNextTime` throttle to govern perception scans).
Concretely that means dropping the `loopNextTime += perceptionTimeClampt()` gate at npc.cpp:3179
so the loop runs each `tickRoutine` (which is already called per frame from `implAiTick`).

However this gate fronts the loop for **every** NPC and every `ZS_*` state, so removing it is a
broad behavioral change (and would make the combat force-call workarounds at 1832/1900/1958 and
the `fastPath` skip redundant). It cannot be validated as a surgical, low-regression fix from
static analysis alone; it needs runtime A/B verification against `Gothic2.exe` across routine,
dialog, and combat states, plus a performance check (per-frame Daedalus loop dispatch for all
in-range NPCs — which is what the original does, but OG's invokeState cost profile differs).

// NOTE: in original-game oCAIHuman::DoAI @0x0069bab0 calls oCNpc_States::DoAIState @0x0076d1a0
// every AI frame, and DoAIState (phase 0) re-invokes funcLoop on every call with no per-state
// interval; perception assessment is throttled separately by the NPC perception timer set in
// oCNpc::SetPerceptionTime @0x0075dba0. OpenGothic instead throttles the state loop itself to
// perceptionTimeClampt() at npc.cpp:3179.
