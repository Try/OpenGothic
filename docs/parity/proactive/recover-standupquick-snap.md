# AI_StandUpQuick wakes/recovers with the full get-up animation instead of snapping

**Confidence:** Medium-High (the divergence is High-confidence from the decompile; the exact
snap-vs-block fix scope carries one small animation-resolution caveat, noted below).

## Original function + address

The two recovery commands route through one message handler but carry **different payloads**:

- `AI_StandUp`  external (Gothic2.exe @ `0x006f4bd0`) builds an `oCMsgMovement` of subtype
  `0xb` (`EV_STANDUP`) and sets the message field at `+0x74` to **1**.
- `AI_StandUpQuick` external (Gothic2.exe @ `0x006f4d30`) builds the same `0xb` message but
  sets field `+0x74` to **0**.

`oCNpc::EV_StandUp` (@ `0x00683ce0`) consumes that flag on the first frame:
`StandUp(this, 0, (msg+0x74 != 0))` — so the message field becomes `StandUp`'s second
parameter. In `oCNpc::StandUp` (@ `0x00682b40`), for a non-swim/non-dive NPC that is not
already standing, after `SetBodyState(BS_STAND)` + `StopTurnAnis`:

- **param_2 == 0** (the *quick* path): it calls `StartAni(anictrl, anictrl+0x1008, -1)` — the
  base standing/idle ani — and returns immediately. No `T_*_2_STAND` get-up transition is
  played, and `EV_StandUp`'s active-animation walk then finds no in-progress transition, so the
  handler reports "done" and the NPC **snaps upright** without blocking the AI queue.
- **param_2 != 0** (the *animated* path, plain `AI_StandUp`): it builds and plays the reverse
  `T_<cur>_2_STAND` get-up transition; `EV_StandUp` keeps returning "not finished" until that
  transition completes, so the get-up animation plays and the AI queue stalls until the NPC is up.

So `AI_StandUp` = animated, queue-blocking get-up; `AI_StandUpQuick` = instant snap to standing.

## OpenGothic file:line

`game/world/objects/npc.cpp:2672-2693` (`Npc::nextAiAction`, `case AI_StandUp` /
`case AI_StandUpQuick`).

## Divergence

OpenGothic falls both labels through to one identical body and never reads the quick flag:

```cpp
case AI_StandUp:
case AI_StandUpQuick: {
  const auto bs = bodyStateMasked();
  ...
  else if(bs==BS_UNCONSCIOUS || bs==BS_LIE) {
    if(!setAnim(Anim::Idle))
      queue.pushFront(std::move(act)); else
      implAniWait(visual.pose().animationTotalTime());   // <-- always the animated+blocking path
  }
  ...
}
```

For a knocked-out (`BS_UNCONSCIOUS`) or sleeping/lying (`BS_LIE`) NPC, `AI_StandUpQuick`
incorrectly plays the full `T_WOUNDED_2_STAND` get-up animation and `implAniWait`s the whole
transition, blocking the AI queue — exactly the behaviour the original reserves for the slow
`AI_StandUp`. The quick variant should instead snap to standing on the same tick (the
`param_2==0` path), which is the whole reason scripts call `AI_StandUpQuick` (forcing a downed/
sleeping NPC up instantly, e.g. cutscene/forced-wake situations). The collapse drops the
distinction entirely. (Note: the companion `BS_SIT` queue-block gap is documented separately in
`wait-standup-from-sit-no-queue-block.md`; this finding is the orthogonal quick-vs-animated split.)

## Proposed patch

`game/world/objects/npc.cpp`, unconscious/lie branch of the standup case:

```cpp
// OLD
      else if(bs==BS_UNCONSCIOUS || bs==BS_LIE) {
        if(!setAnim(Anim::Idle))
          queue.pushFront(std::move(act)); else
          implAniWait(visual.pose().animationTotalTime());
        }

// NEW
      else if(bs==BS_UNCONSCIOUS || bs==BS_LIE) {
        // NOTE: in original-game AI_StandUpQuick (Gothic2.exe @0x006f4d30) sets the EV_STANDUP
        // message flag (+0x74) to 0, which makes oCNpc::StandUp (@0x00682b40) skip the
        // T_*_2_STAND get-up transition and snap straight to the standing idle ani; EV_StandUp
        // (@0x00683ce0) then reports the message finished, so the AI queue is NOT blocked.
        // Plain AI_StandUp (flag==1) plays the animated get-up and blocks the queue until done.
        if(act.act==AI_StandUpQuick) {
          visual.stopAnim(*this,"");
          setStateItem(MeshObjects::Mesh(),"");
          setAnim(Anim::Idle);
          }
        else if(!setAnim(Anim::Idle))
          queue.pushFront(std::move(act)); else
          implAniWait(visual.pose().animationTotalTime());
        }
```

Grep-verified OG symbols: `AI_StandUpQuick` (`game/game/constants.h:365`), `AiQueue::aiStandupQuick`
(`game/world/aiqueue.cpp:175`), `act.act` already read in this case (`npc.cpp:2674-2678`),
`visual.stopAnim`, `setStateItem(MeshObjects::Mesh(),"")`, `setAnim(Anim::Idle)` all used
verbatim in the adjacent general `bs!=BS_DEAD` branch (`npc.cpp:2687-2691`), `implAniWait`
(`npc.cpp`), `BS_UNCONSCIOUS`/`BS_LIE` (`game/game/constants.h`).

**Residual uncertainty (why Medium-High, not High):** the snap branch reuses OpenGothic's
existing "stop ani + set Idle, no wait" primitive (copied from the general standup branch). That
faithfully reproduces the *non-blocking* queue semantics, but whether `setAnim(Anim::Idle)` from
`BS_UNCONSCIOUS` resolves to a true instant stand (vs. still briefly easing through a transition
frame) should be eyeballed in-game on a `Wld_InsertNpc`'d sleeper woken via `AI_StandUpQuick`.
If the visual snap cannot be confirmed, downgrade the patch to **DEFERRED** but keep the
divergence on record — the queue-block difference alone is real.
