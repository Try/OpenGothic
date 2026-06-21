# Issue #857 — Cooking with frying pan/cooker not repeatable in fast mode

## Issue
At a frying pan (and cooker), pressing "up" should cook one piece of meat per
press while the player stays at the station. In OpenGothic the second cook does
nothing — the player must walk away and re-approach to cook again. The original
allowed continuous cooking without re-approaching.

## Subsystem & OG files
- `game/world/objects/npc.cpp:2243-2258` — `ITEM_EXCHANGE` anim-event handler
  (the cooking exchange: clears raw slot, adds cooked item to slot). The
  "fallback for cooking animations" branch (2249-2253) is here.
- `game/world/objects/interactive.cpp` — mobsi interaction state (current
  state index, S0/S1 loop transitions).
- `game/world/objects/npc.cpp` ~2007/2482-2650 — `setInteraction`, AI-state
  transitions driving Use_Mob loop re-entry.
- AI queue: `AI_UseMob` / "T1"/"S1" state advance.

## Original behavior (Gothic2.exe — prose)
Cooking is a mob interaction whose state machine returns to the looped "ready"
state (S1) after each completed cook cycle, so re-pressing "forward" re-enters
the cook transition (e.g. `S1 -> T1 -> S1`) without leaving the mob. The
oCMobInter state index is advanced/reset per cycle; the NPC stays bound to the
mob (`oCNpc::Interact` / mob `BeginUseAnimation`) and a new manipulate message
re-triggers the same transition. The item exchange (raw->cooked) fires once per
cycle and the loop is immediately available again.

## OpenGothic current behavior (file:line)
The single cook cycle works (ITEM_EXCHANGE at npc.cpp:2243-2258 swaps raw for
cooked), but the interaction does not return to a re-triggerable looped state:
after the exchange the player input no longer re-enters the cook transition
while still at the station. The mobsi state in
`game/world/objects/interactive.cpp` is not reset to the S1 "ready" loop in a way
that lets a fresh forward-press re-run the cycle, so a leave/re-approach (which
re-runs the full enter sequence) is the only way to cook again.

## Divergence
Missing loop re-entry: the original keeps the cook mob in a looped ready state
that accepts repeated forward presses; OpenGothic completes one cycle and stalls
until the interaction is re-entered from scratch.

## Disposition: DEFER
This is a mob-interaction state-machine flow bug, not a one-line logic error. The
fix must make the cooking mob return to its looped S1 state after the
ITEM_EXCHANGE cycle and accept a new manipulate/forward input to re-run the
`S1->T1->S1` transition (mirroring smithing/forge loops). That requires editing
the interactive state advancement in `interactive.cpp` plus the Use_Mob
re-trigger in the AI path, and must be verified in-game (cannot repro headless).
Same root cause is shared with the cooking residual noted in #177. No surgical
patch proposed.
