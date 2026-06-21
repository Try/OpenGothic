# AI_StartState clears the AI message queue unconditionally instead of only on the hard-interrupt (behavior==0) path

**Confidence:** Medium-High

## Original function + address (prose)

`oCNpc_States::StartAIState` @ `0x0076cd9a` is the engine entry point that the
`AI_StartState` external (handler `FUN_006e20d0`, reachable from the `AI_StartState`
external string @ `0x008b5374`) ultimately drives. The external packs an `oCMsgState`
message: its 4-argument constructor (`oCMsgState::oCMsgState` @ `0x007680c0`) stores the
script-supplied state function at message offset `+0x44` and the script-supplied
*behavior* integer's bit 0 at message offset `+0x70` bit0. When the event manager later
dispatches that message, `oCNpc::EV_DoState` @ `0x00756800` calls
`StartAIState(states, stateFunc, param_2 = (msg->0x70 << 31) >> 31, 0, 0.0, param_5 = …)`,
i.e. `param_2` is exactly **behavior bit 0**.

Inside `StartAIState @ 0x0076cd9a` the tail branches on `param_2`:

- `param_2 == 0` (behavior bit0 == 0 -> "finalize / hard restart"): sets phase
  (`this+0x30`) = 2, started (`this+0x34`) = 0, and -- provided the NPC is not currently
  bound to a mob interaction -- runs `zCModel::StopAnisLayerRange(2,0x100)`,
  `oCNpc::Interrupt(0,1)`, and **`oCNpc::ClearEM(...)`** (the purge of the pending AI/EM
  message queue).
- `param_2 != 0` (behavior bit0 == 1 -> "soft continue", used by routine activation,
  `oCNpc_States::ActivateRtnState @ 0x0076c330` which calls `StartAIState(.., 1, ..)`, and
  by scripts that pass behavior `1`): sets phase (`this+0x30`) = 1 and does **not** clear
  the EM queue, does **not** Interrupt, does **not** stop the animation layer.

So in the original engine the pending AI message queue is purged **only** on the
behavior==0 hard-interrupt path; the behavior==1 soft path preserves any queued AI
messages.

## OpenGothic file:line

`game/world/objects/npc.cpp:2959` — `Npc::startState(ScriptFn id, std::string_view wp,
gtime endTime, bool noFinalize)` calls `clearAiQueue();` **unconditionally** (before the
`noFinalize` flag is consulted), immediately followed by `clearState(noFinalize);`.

Mapping: OpenGothic's `Npc::ai_startstate`/`AiQueue::aiStartState` carries the script
behavior in `act.i0`, and `Npc::nextAiAction` (`game/world/objects/npc.cpp:2564`) calls
`startState(act.func, act.s0, aiState.eTime, /*noFinalize=*/act.i0==0)`. Hence
`noFinalize==true` <-> behavior==0 <-> the original's *hard-interrupt* path
(`ClearEM` runs), and `noFinalize==false` <-> behavior==1 <-> the original's *soft* path
(`ClearEM` does not run).

## Divergence

OpenGothic wipes the AI action queue on **every** `startState` transition, whereas the
original purges the message queue only on the behavior==0 hard-interrupt branch. When a
state is (re)started with behavior `1` -- the soft path used by routine activation,
`AI_SetNpcsToState` (`game/world/objects/npc.cpp:2860`, which pushes
`aiStartState(act.func, 1, ...)`), and any script doing `AI_StartState(self, ZS_X, 1, wp)`
-- OpenGothic discards still-pending AI actions that the original would have kept and
continued to process, changing the AI message processing order/contents around soft state
switches.

## Proposed patch

Gate the queue purge on the hard-interrupt (`noFinalize`) flag so it mirrors the original's
`param_2==0`-only `ClearEM`. `clearAiQueue()`, `clearState(bool)` and the `noFinalize`
parameter all exist and are grep-verified in `game/world/objects/npc.cpp`
(`clearAiQueue` @ line 4592; `clearState(bool noFinalize)` @ line 2997; `startState(...,
bool noFinalize)` @ line 2945).

OLD (`game/world/objects/npc.cpp:2959-2960`):
```cpp
  clearAiQueue();
  clearState(noFinalize);
```

NEW:
```cpp
  // NOTE: in original-game oCNpc_States::StartAIState @0x0076cd9a the EM/AI message queue
  // (oCNpc::ClearEM) is purged only on the hard-interrupt path (StartAIState param_2==0,
  // i.e. behavior bit0==0 -> noFinalize). The soft path (behavior==1, used by routine
  // activation / AI_SetNpcsToState) preserves pending AI messages.
  if(noFinalize)
    clearAiQueue();
  clearState(noFinalize);
```

**Status: DEFERRED for the edit (documentation-only), recommended for review.**
Reason: although the binary asymmetry (ClearEM gated on `param_2==0`) is unambiguous, the
original's hard path also runs `oCNpc::Interrupt(0,1)` and `StopAnisLayerRange` which
OpenGothic's `startState` does not model at all, so the two engines already diverge in how
"hard vs soft" is realized. OpenGothic additionally relies on `clearAiQueue()` resetting
`waitTime`/`aniWaitTime`/`faiWaitTime`/look-at state, and several documented workarounds
(e.g. the `funcIni==id` short-circuit at line 2949, the `resumeAiRoutine` path at line
3420 which uses `noFinalize=false`) assume the queue is always cleared. Making the clear
conditional is a behaviorally correct alignment but warrants targeted in-game verification
(routine transitions, `AI_SetNpcsToState` crowd scenes, dialog hand-offs) before applying,
since it can resurrect previously-dropped AI actions during soft state switches.
