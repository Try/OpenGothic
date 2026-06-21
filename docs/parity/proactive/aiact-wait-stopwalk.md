# AI_Wait does not halt an in-progress walk/turn animation

Confidence: Medium

## Original function
oCNpc::EV_Wait @ 0x00756820 (Gothic2.exe).

The original wait handler runs every tick while the wait timer is active. Before
decrementing the remaining wait time it inspects the animation controller: if the
NPC is currently walking, or is in the swim state, or is in the dive state, it
forces the NPC to stop (the controller's "stand" transition) and unconditionally
cancels any in-progress turn animations. Only after that does it subtract the
frame delta from the remaining wait time and report completion when the timer
reaches zero. The net effect is that the moment an NPC begins an AI_Wait it stops
moving and stops turning, then idles in place for the requested duration.

## OpenGothic
game/world/objects/npc.cpp:2584-2586 — the `AI_Wait` case only calls
`implAiWait(act.i0)`, which records the wait deadline. No call stops the walk or
turn animation.

The per-tick wait gate at game/world/objects/npc.cpp:2395-2401 advances movement
with `mvAlgo.tick(dt, MoveAlgo::WaitMove)`. `MoveAlgo::npcMoveSpeed`
(game/game/movealgo.cpp:548-561) still applies `animMoveSpeed(dt)` under
`WaitMove` (it only skips the go-to steering term). So if a walk/sneak animation
was active when AI_Wait fired, the NPC keeps sliding forward for the entire wait,
and a turn animation in progress keeps rotating.

## Divergence
Original: AI_Wait immediately stops walk and turn animations, then idles.
OpenGothic: AI_Wait leaves the active walk/turn animation running, so the NPC
drifts/rotates during the wait. Visible when AI_Wait follows movement in a routine
without an intervening explicit stop.

## Proposed patch
File: game/world/objects/npc.cpp

OLD:
```cpp
    case AI_Wait:
      implAiWait(uint64_t(act.i0));
      break;
```

NEW:
```cpp
    case AI_Wait:
      // NOTE: in original-game (oCNpc::EV_Wait @0x00756820) the wait handler
      // forces the npc to stop walking and cancels any in-progress turn anim
      // before counting down the timer, so the npc idles in place.
      stopWalkAnimation();
      implAiWait(uint64_t(act.i0));
      break;
```
