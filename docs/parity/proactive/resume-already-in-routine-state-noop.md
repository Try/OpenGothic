# Routine-resume re-enters the daily-routine state even when the NPC is already running it

**Confidence:** Medium-High (code-logic divergence is certain; in-game visibility depends on a script issuing `Npc_ContinueRoutine` while the NPC is still in its routine state, which is plausible but not enumerated to a specific daily script here).

## Original function + address (prose)

`AI_ContinueRoutine` is delivered as a state-message with an empty target function. In
`oCNpc::EV_DoState` (entry `0x00756600`) the message is dispatched: when the message's
state-function field is `0`, it calls `oCNpc_States::StartRtnState` (entry `0x0076c2e0`)
with the force-flag `0`. `StartRtnState` in turn calls `oCNpc_States::ActivateRtnState`
(entry `0x0076c330`) with `param == 0` (the "resume / not forced" path).

In `ActivateRtnState`'s resume path (`param == 0`), after the event-manager-empty check the
function computes the NPC's currently-active state index (from the running-state field, the
internal `+0x1c` "current state func") and the daily-routine's target state index
(`oCRtnEntry::GetState` of the current routine entry at `+0xa4`). It then performs an early
equality check: **if the currently-active state already equals the routine's target state,
it returns success (`1`) immediately — without finalizing and re-entering the state.** Only
when the active state differs does it proceed to start (restart) the routine state via
`StartAIState`. The forced path (`param != 0`, used by `RestartRoutines` / time-change /
`UpdateSingleRoutine`) has no such early-out and always (re)starts.

## OpenGothic file:line

`game/world/objects/npc.cpp:3531` — `Npc::resumeAiRoutine()` (reached from
`game/world/objects/npc.cpp:2926`, the `AI_ContinueRoutine` case).

```cpp
void Npc::resumeAiRoutine() {
  clearState(false);
  auto& r = currentRoutine();
  if(r.callback.isValid()) {
    auto t = endTime(r);
    startState(r.callback,r.wayPointName(),t,false);
    }
  }
```

## Divergence

OpenGothic unconditionally calls `clearState(false)` before re-starting. `clearState(false)`
finalizes the currently-running state (it invokes the state's `funcEnd` when
`aiState.funcIni.isValid() && aiState.started`) and then wipes `aiState`. The subsequent
`startState` therefore cannot hit its own same-state guard (`if(aiState.funcIni==id)`,
npc.cpp:3036) because `aiState` was already cleared, so it always re-initialises the state
(`started=false`), causing `funcIni` to run again next tick.

Net effect: when `Npc_ContinueRoutine` is issued while the NPC is *already* in its
daily-routine state (e.g. a perception/defensive script that ends with `AI_ContinueRoutine`
without ever having switched states), OpenGothic runs `ZS_<routine>_End` followed by a fresh
`ZS_<routine>` entry — re-equipping item-states, restarting the goto-waypoint and ambient
animation. The original no-ops and the routine continues seamlessly.

## Proposed patch

Add the original's "already in the routine state -> no-op" early-out before `clearState`,
so the resume of a state the NPC is already running does nothing (matching `ActivateRtnState`).

OLD (`game/world/objects/npc.cpp:3531`):
```cpp
void Npc::resumeAiRoutine() {
  clearState(false);
  auto& r = currentRoutine();
  if(r.callback.isValid()) {
    auto t = endTime(r);
    startState(r.callback,r.wayPointName(),t,false);
    }
  }
```

NEW:
```cpp
void Npc::resumeAiRoutine() {
  auto& r = currentRoutine();
  if(r.callback.isValid() && aiState.funcIni==r.callback) {
    // NOTE: in original-game oCNpc_States::ActivateRtnState @0x0076c330 the resume path
    // (force-flag 0, reached via AI_ContinueRoutine -> EV_DoState @0x00756600 ->
    // StartRtnState @0x0076c2e0) returns success without re-entering when the currently
    // active AI-state already equals the daily-routine state. Re-entering would finalize
    // (funcEnd) and re-init (funcIni) the same routine state, glitching the routine.
    if(!r.wayPointName().empty())
      hnpc->wp = r.wayPointName();
    return;
    }
  clearState(false);
  if(r.callback.isValid()) {
    auto t = endTime(r);
    startState(r.callback,r.wayPointName(),t,false);
    }
  }
```

Grep-verified symbols: `Npc::resumeAiRoutine` (npc.cpp:3531), `aiState.funcIni`
(npc.cpp:3036/3074), `Npc::currentRoutine` (npc.cpp:3394), `Routine::callback` &
`Routine::wayPointName()` (npc.h:428/432), `ScriptFn::isValid`/`operator==` (used at
npc.cpp:3033/3036), `hnpc->wp` (npc.cpp:3042/3049). The `wp` re-assignment mirrors the
waypoint write the original still performs on the resume path.
