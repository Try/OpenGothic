# AI_StandUp from a ground-sit does not block the AI queue for the get-up animation

**Confidence:** Medium-High (divergence is High-confidence; the fix scope carries one small uncertainty, noted below).

## Original function + address

`oCNpc::EV_StandUp` (Gothic2.exe @ `0x00683ce0`) is the per-frame message handler for the
`AI_StandUp` / `AI_StandUpQuick` AI message. On the first frame it calls
`oCNpc::StandUp` (@ `0x00682b40`), which — for a non-swimming, non-diving NPC that is not
currently standing — sets body-state `BS_STAND`, stops any turn animation, and kicks off the
stand-up animation (`StartAni` of the controller's stand-ani id, field `+0x1008`, i.e. the
`T_*_2_STAND` / sit→stand transition).

The important part is the handler's **return value**: after starting the stand-up,
`EV_StandUp` walks the model's active-animation list and, while the stand-up animation is
still playing, returns `1` ("message not finished"); it only returns `0` ("done") once the
get-up animation has completed. In ZenGin a message that returns "not finished" stays at the
head of the EM queue, so **the next queued AI action cannot run until the NPC has visibly
finished standing up.** (Swim state 5 and dive state 6 likewise return `1` until the
surface/get-up transition completes.)

## OpenGothic file:line

`game/world/objects/npc.cpp:2657-2678` (`Npc::nextAiAction`, `case AI_StandUp` / `AI_StandUpQuick`).

## Divergence

OpenGothic reproduces the queue-blocking behaviour only for the lie/sleep/unconscious path:

```cpp
else if(bs==BS_UNCONSCIOUS || bs==BS_LIE) {
  if(!setAnim(Anim::Idle))
    queue.pushFront(std::move(act)); else
    implAniWait(visual.pose().animationTotalTime());   // <-- blocks queue for the get-up anim
  }
else if(bs!=BS_DEAD) {                                  // <-- BS_SIT lands here
  visual.stopAnim(*this,"");
  setStateItem(MeshObjects::Mesh(),"");
  setAnim(Anim::Idle);
  // no implAniWait() -> queue advances on the SAME tick
  }
```

A ground-sitting NPC (`BS_SIT`, set by the sit overlay/animation, e.g. campfire/round-fire
sitters with no interactive MOBSI — the MOBSI bench/throne case is handled by the
`interactive()!=nullptr` branch above) falls into the general `bs!=BS_DEAD` branch. There
OpenGothic starts the idle/get-up animation but does **not** call `implAniWait`, so the AI
queue is not blocked: the very next queued action (e.g. the `AI_GotoWP` that follows
`AI_StandUp` in a routine change, or B_AssessTalk's turn-to-player) is dequeued on the same
tick and overrides the sit→stand transition. The NPC snaps out of the sit instead of playing
the get-up animation, whereas the original blocks the queue until the transition finishes.

`BS_LIE`/`BS_UNCONSCIOUS` were given the wait, but the equally-transitional `BS_SIT` case was
missed. `BS_STAND`/`BS_WALK`/`BS_RUN` must *not* wait (no get-up transition; waiting on the
idle-loop length would wrongly stall the queue), which is why the fix is scoped to `BS_SIT`
rather than applied to the whole branch.

## Proposed patch

`game/world/objects/npc.cpp`, general standup branch:

```cpp
// OLD
      else if(bs!=BS_DEAD) {
        visual.stopAnim(*this,"");
        setStateItem(MeshObjects::Mesh(),"");
        setAnim(Anim::Idle);
        }

// NEW
      else if(bs!=BS_DEAD) {
        visual.stopAnim(*this,"");
        setStateItem(MeshObjects::Mesh(),"");
        setAnim(Anim::Idle);
        // NOTE: in original-game oCNpc::EV_StandUp (Gothic2.exe @0x00683ce0) the standup
        // message returns "not finished" while the get-up animation is still playing, so the
        // AI queue stalls until the NPC has stood up. Mirror that for a ground-sit (BS_SIT);
        // an already-standing/walking NPC has no transition and must not stall.
        if(bs==BS_SIT)
          implAniWait(visual.pose().animationTotalTime());
        }
```

Grep-verified symbols: `BS_SIT` (`game/game/constants.h:176`), `implAniWait`
(`game/world/objects/npc.cpp:1937`), `visual.pose().animationTotalTime()` (used identically in
the adjacent `BS_LIE` branch, `npc.cpp:2670`), `bodyStateMasked()` (`npc.cpp:2659`).

**Residual uncertainty (why Medium-High, not High):** the patch assumes `setAnim(Anim::Idle)`
from `BS_SIT` resolves to the sit→stand transition so that `animationTotalTime()` returns that
transition's length (as it already does for `BS_LIE`). If a given model lacks a `T_*_2_STAND`
sit transition, `animationTotalTime()` would instead be the idle-loop length and the queue
would over-stall; this should be confirmed in-game (campfire sitter changing routine) before
merge. If that cannot be confirmed, treat as **DEFERRED**.
