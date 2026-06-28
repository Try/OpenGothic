# dlgturn-turntonpc-bodystate-gate

**Confidence:** Medium-High (decompile unambiguous and grep-verified in two handlers; practical dialog frequency is moderate — matters when the turn arrives while the NPC is not already idle/running).

## Original fn + address

`oCNpc::EV_TurnToVob` @ `0x00686160` is the engine handler for the `AI_TurnToNpc`
script external (it backs `oCMsgMovement` sub-type *TurnToVob*). Before performing
any rotation it reads the masked body-state via `oCNpc::GetBodyState` @ `0x0075eae0`
(which returns `bodyStateWord & 0x7f`, i.e. the enum value without the
interruptable/freehands flag bits) and gates the whole turn:

- If `bodyState != 0` (BS_STAND) **and** `bodyState != 3` (BS_RUN), it returns
  immediately (message consumed, **no turn happens at all**).
- Otherwise, if the animation controller is not already standing
  (`oCAniCtrl_Human::IsStanding`), it forces `oCNpc::StandUp(0,1)` and then turns.

The sibling handler `oCNpc::EV_TurnAway` @ `0x00685ec0` (`AI_TurnAway`) uses the
*same* pattern but is stricter: it turns **only** when `bodyState == 0` (BS_STAND),
returning without effect for every other state. By contrast `oCNpc::EV_Turn`
@ `0x00685de0` and `oCNpc::EV_TurnToPos` @ `0x00686070` carry **no** such gate —
confirming the body-state restriction is specific to the *turn-toward-an-NPC*
family (the conversation-facing commands), not a generic turn guard.

## OG file:line

`/Users/admin/Downloads/opengothic/game/world/objects/npc.cpp:2666-2691`
(the `AI_TurnAway` and `AI_TurnToNpc` cases in `Npc::nextAiAction`), which both
route through `Npc::prepareTurn()` at `npc.cpp:2634-2646`.

## Divergence

OpenGothic funnels both `AI_TurnToNpc` and `AI_TurnAway` through the single shared
`prepareTurn()` helper, whose only body-state logic is:

- `BS_WALK` / `BS_SNEAK` → stop the walk anim, return `false` → the action is
  **re-queued (pushFront) and retried next tick**, so the NPC stops walking and
  then turns;
- every other state → return `true` → the turn proceeds unconditionally.

This differs from the original in two concrete ways:

1. **Non-stand/run states turn anyway.** For e.g. `BS_SWIM`, `BS_DIVE`,
   `BS_CLIMB`, `BS_MOBINTERACT`, the original drops the turn (message consumed,
   no rotation); OpenGothic performs it.
2. **Walk is waited-out instead of dropped.** With `BS_WALK`, the original
   `AI_TurnToNpc`/`AI_TurnAway` is a no-op (the separate movement message keeps
   the NPC walking); OpenGothic instead stops the walk and turns a tick later.
3. **`AI_TurnAway` is over-permissive.** The original allows it only in
   `BS_STAND`; OpenGothic also allows `BS_RUN` (and the swim/etc. states), because
   it shares the looser `AI_TurnToNpc` gate.

`AI_TurnToNpc` is the canonical "face the conversation partner" command issued by
`B_AssessTalk` / `ZS_Talk` and many dialog routines, so the gate governs the
dialog turn-to-speaker behavior. (The secondary `StandUp`-when-not-standing step
is intentionally **not** reproduced below — `IsStanding`/`StandUp` semantics were
already partially addressed by the `AI_StandUp` parity note at `npc.cpp:2819-2828`,
and folding it in here risks the same over-clear-of-effects regression that note
warns about.)

## Proposed patch

Add the original per-case body-state gates ahead of `prepareTurn()`. `BS_STAND`
and `BS_RUN` exist in `game/game/constants.h:162,168`; `bodyStateMasked()` is
`npc.cpp:3641`.

```cpp
    case AI_TurnAway: {
      // NOTE: in original-game oCNpc::EV_TurnAway @0x00685ec0 the turn is performed ONLY when
      // GetBodyState()==BS_STAND(0); any other body-state consumes the message without turning.
      if(bodyStateMasked()!=BS_STAND)
        break;
      if(!prepareTurn()) {
        queue.pushFront(std::move(act));
        break;
        }
      if(act.target!=nullptr && implTurnAway(*act.target,dt)) {
        queue.pushFront(std::move(act));
        break;
        }
      break;
      }
    case AI_TurnToNpc: {
      // NOTE: in original-game oCNpc::EV_TurnToVob @0x00686160 the turn-to-conversation-partner is
      // performed ONLY when GetBodyState() is BS_STAND(0) or BS_RUN(3); every other state (walk,
      // sneak, swim, climb, mobinteract, ...) consumes the message and does NOT rotate the NPC.
      // OpenGothic's shared prepareTurn() instead waited-out walk/sneak and turned in all other
      // states, so AI_TurnToNpc rotated NPCs in body-states the original never turns in.
      if(bodyStateMasked()!=BS_STAND && bodyStateMasked()!=BS_RUN)
        break;
      if(!prepareTurn()) {
        queue.pushFront(std::move(act));
        break;
        }
      if(act.target!=nullptr && implTurnTo(*act.target,dt)) {
        queue.pushFront(std::move(act));
        break;
        }
      // Not looking quite correct in dialogs, when npc turns around
      // Example: Esteban dialog
      // currentLookAt    = nullptr;
      // currentLookAtNpc = nullptr;
      break;
      }
```
