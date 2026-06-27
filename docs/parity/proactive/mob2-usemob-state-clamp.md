# MOBSI parity: AI_UseMob target-state not clamped to stateNum (soft-lock on out-of-range state)

**Confidence:** Medium (clear, faithful-to-original logic divergence; impact requires a script/content `AI_UseMob` call whose target state exceeds the mob's `stateNum`, which the original silently tolerates and OpenGothic turns into a hard AI soft-lock).

## Original function + address

`oCMobInter::AI_UseMobToState(oCNpc*, int targetState)` at `Gothic2.exe 0x00721f00`.

In the branch that drives a scripted NPC toward a requested mob state, the original first clamps the
target: if the mob's maximum state index (`stateNum`, stored at `this+0x1F8`) is less-or-equal to the
requested target, the target is overwritten with `stateNum`. In prose: `target = min(target, stateNum)`.
A target of `-1` is left untouched by that `min` (it is less than `stateNum`) and routes to the
detach/stand path. The clamped target is what the per-step mob-message machinery then walks toward, so a
script that asks for a state index above the mob's real top simply lands on the top state and the
`AI_UseMob` command completes normally.

## OpenGothic file:line

`game/world/objects/npc.cpp:2799` (the `AI_UseMob` case of the AI-queue processor).

The requested state arrives unmodified: `GameScript::ai_usemob` (`game/game/gamescript.cpp:3326`) →
`AiQueue::aiUseMob` (`game/world/aiqueue.cpp:213`) stores the raw script argument in `act.i0` with no
clamp. The completion check is:

```cpp
if(currentInteract==nullptr || currentInteract->stateId()!=act.i0) {
  queue.pushFront(std::move(act));
  return;
  }
```

`Interactive::stateId()` returns `state`, and `state` is hard-clamped to `[0, stateNum]` by every
`setState(std::min(stateNum, state+1))` in `implTick` (`interactive.cpp:356,423`). The negative/detach
target is already handled separately at `npc.cpp:2754` (`if(act.i0<0)`), so this branch only sees
targets `>= 0`.

## Divergence

When a script issues `AI_UseMob(self, "SCHEME", N)` with `N > stateNum`, the mob advances the attached
NPC to its terminal state `stateNum` and parks there (`loopState`), so `stateId()` saturates at
`stateNum` and can never equal `N`. The completion test `stateId()!=act.i0` is therefore permanently
true: OpenGothic re-pushes the same command to the front of the AI queue and `return`s every tick. The
NPC is stuck on the mob forever and its entire AI queue stalls. The original clamps the target to
`stateNum`, so the same NPC reaches the top state, the command completes, and the queue proceeds.

## Proposed patch

`game/world/objects/npc.cpp:2799` — compare against the clamped target. `stateCount()` is grep-verified
in `interactive.h:55` (`int32_t stateCount() const { return stateNum; }`); `stateId()` in
`interactive.h:54`.

OLD:
```cpp
      if(currentInteract==nullptr || currentInteract->stateId()!=act.i0) {
        queue.pushFront(std::move(act));
        return;
        }
```

NEW:
```cpp
      // NOTE: in original-game oCMobInter::AI_UseMobToState (Gothic2.exe 0x00721f00) clamps the
      // requested target to the mob's top state index (target = min(target, stateNum)) before
      // stepping toward it. Without the clamp an AI_UseMob target above stateNum saturates at
      // stateNum but never equals act.i0, so OpenGothic re-pushed the command forever and the NPC
      // soft-locked on the mob. (act.i0<0 detach is handled above.)
      const int32_t goal = std::min(act.i0, currentInteract!=nullptr ? currentInteract->stateCount() : act.i0);
      if(currentInteract==nullptr || currentInteract->stateId()!=goal) {
        queue.pushFront(std::move(act));
        return;
        }
```
