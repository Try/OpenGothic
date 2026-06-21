# Issue #707 — Implement "edit focus" in marvin-mode

**Category:** marvin / debug feature
**Disposition:** DEFER (new feature; needs UI + vob-edit infra; not a parity bug)

## Request
Implement the original marvin "edit focus" command (focus-vob editing) in
OpenGothic's marvin mode. The thread also mentions a side report of enemies
ceasing to attack after long marvin sessions + a save the user wants repaired,
but the titled task is the edit-focus command.

## OG files
- `game/marvin.cpp` — command table at `Cmd cmd = {...}` (60–153) and the
  dispatch `switch(ret.cmd.type)` (259+). There is **no** `edit focus` /
  `editfocus` entry in the table today; the original's vob-edit commands
  (`ztoggle vobmorph`, `zoverlaymds`, many edit ops) are present only as
  `C_Invalid` stubs (marvin.cpp:68–104).
- `game/marvin.h` — `enum CmdType` (the `C_*` command ids).
- Focus resolution: `game/world/focus.cpp`, `Npc`/`Interactive` focus in
  `game/game/playercontrol.cpp`.

## Original behavior (prose)
In the original engine "marvin" (god/dev mode) exposes a spacer-like focus
editor: with a vob under the crosshair, the player can translate/rotate/inspect
the focused vob and tweak its properties live. It is a developer tool, not part
of normal gameplay, and is gated behind marvin mode.

## OG current state
Marvin mode exists (`Marvin::recognize` / `exec`) and implements a subset:
goto, cheat full/god, kill, insert, set time, set/print var, toggle frame/
camera/GI/vsm, ztrigger, etc. The large majority of original marvin/spacer
commands — including all vob-edit/focus-edit ones — are stubbed `C_Invalid`.
Edit-focus specifically is **absent**.

## Divergence
OG marvin lacks the focus/vob editing command entirely.

## Why DEFER (feature, not surgical parity FIX)
Implementing edit-focus requires: (1) a new `C_EditFocus` command + parser
entry, (2) hooking the current focus vob (focus.cpp / playercontrol focus),
(3) interactive transform/inspect UI and input handling, (4) persistence of
edits. This is sizeable new infrastructure, overlaps the broader #215 "finalize
marvin" effort, and has no automated way to verify against the original. Should
be scheduled as part of #215, not as an isolated quick fix.

The "enemies stopped attacking" save-corruption side report is a separate
runtime issue and would need the attached save to investigate; out of scope for
this card.
