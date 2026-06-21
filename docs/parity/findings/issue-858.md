# Issue #858 — Rupert keeps peeing during talk/trade

## Issue
NPC Rupert continues his urinate scheme animation while the player talks/trades
with him. In the original he stops (leaves the interactive / cancels the scheme
animation) before speaking, so both animations don't play at once.

## Subsystem & OG files
- `game/world/objects/npc.cpp:2007`, `:2482-2650` — `setInteraction`,
  AI-state transitions that quit a mob interaction.
- `game/world/objects/npc.cpp:525-527` `stopDlgAnim`, `:1947`, `:2420-2421`.
- `game/world/objects/npc.cpp:486`, `:608` `setInteraction(nullptr,true)`.
- Dialogue start path: `AI_StartState ZS_Talk` / `oCInfoManager`-driven dialog
  begin (gamescript dialog externals) — where the talked-to NPC should be pulled
  out of its routine scheme animation.
- `game/world/objects/interactive.cpp` — scheme/mobsi state for the pee
  interactive (a routine TA mob like a wall/bush).

## Original behavior (Gothic2.exe — prose)
When a dialog starts, the engine forces the addressed NPC into `ZS_Talk`, which
(via the body-state / interaction cleanup at state entry) detaches it from any
active mob interaction and cancels routine scheme animations — `oCNpc` quits the
oCMobInter (the equivalent of `setInteraction(nullptr)`) and resets body state so
the urinate loop animation stops before the talk/dialog animation plays. The
scheme animation and dialog animation are mutually exclusive.

## OpenGothic current behavior (file:line)
OpenGothic interrupts walking/dialog animations on state changes
(`stopWalkAnim`/`stopDlgAnim` at npc.cpp:1947, 2420-2421) and quits interactions
on many transitions via `setInteraction(nullptr)` (2482-2650). But the
dialog-start path for the *addressed* NPC does not reliably force it out of its
current routine scheme/mob interaction before the talk animation begins, so a
looping scheme animation (Rupert's pee) keeps playing under the dialog. There is
no equivalent of "on entering ZS_Talk, detach from current interactive + reset
body state" for the conversation partner.

## Divergence
Missing forced detach of the talked-to NPC from its active routine interactive at
dialog/ZS_Talk entry; the original cancels the scheme animation before talking.

## Disposition: DEFER
Requires adding a `setInteraction(nullptr)` + body-state reset for the addressed
NPC at dialog start (ZS_Talk entry), without breaking NPCs that legitimately talk
while seated/at a mob (smith, alchemist, throne) — so it needs a scheme/body-state
predicate and in-game verification across many NPCs. Not a safe one-liner; no
surgical patch proposed. Hook point: the dialog-begin/ZS_Talk transition in
`npc.cpp` (around the existing `setInteraction`/`stopDlgAnim` calls).
