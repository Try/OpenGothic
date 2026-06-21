# Issue #719 — Changing a global script var does not re-trigger an important info

**Category:** scripting / dialog re-evaluation
**Disposition:** DEFER (real bug; needs runtime AI-state work + in-game repro)

## Repro (from reporter)
Gothic 1, monastery ruin. Player turns into a meatbug, triggers the gate
mechanism so the global `MonasteryRuin_GateOpen` is set. Gorn has an *important*
info whose `condition` depends on that var.
- If the gate is opened **before** Gorn arrives at the gate waypoint, his
  important-info sequence starts correctly.
- If Gorn has **already arrived** at the gate (entered his idle/standing state),
  opening the gate no longer makes him resume the scripted info — he never
  re-assesses.

## OG files
- `game/world/objects/npc.cpp` — `Npc::startDialog` (4192), perception dispatch
  `perceptionProcess` (4199+), AI-state tick `tickAiState`/`nextAiAction`.
- `game/game/gamescript.cpp` — `npc_checkinfo` (2519, the `Npc_CheckInfo`
  external) and `dialogChoices` (863, important loop at 877).
- `game/game/aistate.cpp` — ZS state loop / routine handling.

## Original behavior (prose, clean-room)
In the original, important-info auto-start is **script-driven**: the ZS_TALK /
`B_AssessTalk` machinery calls `Npc_CheckInfo(self, 1)` repeatedly while the NPC
is in a state that allows talking. `oCInformationManager::ProcessImportant` /
`ProcessNextImportant` (Gothic2.exe `0x006615b0` / `0x006617b0`) and
`CollectInfos` (`0x00661aa0`) collect and start importants once dialogue is
engaged; the condition functions are evaluated **live** at that moment, so a
freshly-changed global is observed. The recurring trigger comes from the NPC's
AI/perception loop continuing to fire `PERC_ASSESSTALK` (`AssessTalk_S`
`0x0075c890`) every perception tick while the NPC is idle-and-able-to-talk.

The defect is therefore not in condition evaluation (that already reads live
globals in OG — see `npc_checkinfo` at 2519, which calls the condition symbol
each invocation). It is that once Gorn reaches the gate and settles into his
arrival state, OG stops re-issuing the ASSESSTALK perception (or the routine
state does not loop back to a talk-capable idle), so `Npc_CheckInfo` is never
called again and the now-true condition is never noticed.

## OG current state
- `Npc::startDialog` (npc.cpp:4195) only ever fires `PERC_ASSESSTALK` when the
  player explicitly engages; there is no engine-side periodic re-assessment of
  important infos for a standing NPC the way the original perception loop does.
- Condition re-evaluation itself is correct (`npc_checkinfo` recomputes each
  call), so the fix must restore the *recurring assessment*, not the check.

## Divergence
OG does not periodically re-run important-info assessment for an NPC that has
finished moving and entered an idle/arrival state; the original keeps
ASSESSTALK perception alive so the condition is re-checked and the important
info auto-starts when the global flips.

## Why DEFER
Requires reproducing the monastery scene and tracing which perception/AI tick
the original keeps alive vs. OG. Touching the perception loop is high-risk
(affects every NPC's talk/idle behavior) and cannot be made surgically without
runtime verification. Recommended next step: instrument `perceptionProcess` for
PERC_ASSESSTALK frequency on a standing NPC and compare against original.
