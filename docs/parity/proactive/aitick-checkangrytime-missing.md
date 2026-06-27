# AI-tick parity sweep: per-frame `CheckAngryTime` decay is absent in OpenGothic

**Confidence:** Low-Medium (real, grep-verified gap; but DEFERRED — not a surgical, build-verifiable fix)

## Scope of this sweep

Fresh re-comparison of the OpenGothic NPC per-frame AI tick
(`Npc::tick` / `Npc::nextAiAction` / `Npc::implAiTick` / `Npc::tickRoutine`,
`game/world/objects/npc.cpp`) against the original `oCAIHuman::DoAI`
(Gothic2.exe @0x0069bab0) and the state machine it drives,
`oCNpc_States::DoAIState` @0x0076d1a0 / `ActivateRtnState` @0x0076c330 /
`StartAIState` @0x0076cd9a, plus the wait/output event handlers
`oCNpc::EV_Wait` @0x00756820 and `oCNpc::EV_WaitTillEnd` @0x0075a5b0.

For the eight specifically-requested sub-areas the OpenGothic code reads as
faithful or already carries parity fix-notes:

- **AI-state loop/end timing** — `tickRoutine` (npc.cpp:3049) collapses the
  original phase machine (`this+0x30` = -1 init / 0 loop / 1 end) into
  ini/loop/end with `loopNextTime` throttling; the `LOOP_CONTINUE=0 / LOOP_END=1`
  mapping and the `hasZSStateLoop()` no-loop-state behaviour match the original
  phase-0 `funcLoop<1 ? 1` path.
- **wait / anim-wait** — `EV_Wait` (`_Stand` + `StopTurnAnis` + countdown) is
  mirrored by the AI_Wait `stopWalkAnimation()` + `implAiWait` path (already
  fixed: AI_Wait stopWalk) and the wait block at npc.cpp:2419.
- **overlay-AI queue** — `nextAiAction(aiQueueOverlay,dt)` (npc.cpp:2407) runs
  independently of the wait gate, consistent with overlay-SVM being separate
  from the main EM queue (aiOutputBarrier persist already fixed).
- **daily-routine re-eval** — driven by `eTime<=time` + empty-target +
  `outputPipe->isFinished()` (npc.cpp:3098) then `currentRoutine()`
  re-selection; the max-start fallback already carries a fix-note.
- **far/near policy** — `worldobjects.cpp:224-239` 3000/6000 distance bands are
  a documented HACK approximation of the spawn-manager active-vob range.
- **turn-to-fai / body-state gating / output pipe** — `implTurnToFai`,
  `bodyStateMasked()` gates, and `performOutput`/`aiOutputBarrier` read faithful.

## The one concrete divergence found

**Original function + address:** `oCNpc::CheckAngryTime` @0x00730670, called
unconditionally once per frame from `oCAIHuman::DoAI` @0x0069bab0 (call site
@0x0069befd, near the top of every AI tick, right after `*(this+0x178)=0` and
before `CorrectAniStates`). The routine decays a per-NPC "angry/threat" timer
(seeded from `NPC_ANGRY_TIME`) toward a target level: while the current level
(field `this+0x7e8`) differs from the pending level (`this+0x7e4`) it subtracts
the frame delta each tick; on expiry it commits the pending level, clamps it to
[0,4], toggles a perception/news flag bit (`this+0x75c & 4`), and resets the
timer. This is the slow decay of an NPC's accumulated provocation/threat level.

**OpenGothic file:line:** no equivalent. `grep -rin "angry\|angrytime\|threat.*time\|provok" game/`
finds only the unrelated `ATT_ANGRY` attitude enum (`game/game/constants.h:246`);
there is no per-frame threat-level decay anywhere in the tick path.

**Divergence:** OpenGothic never decays an accumulated threat/angry level over
time, so the original's gradual "cool-down" of a provoked NPC (and the
associated `0x75c` flag transition) does not occur.

## Proposed patch

**DEFERRED.** Reason: this is a missing subsystem, not a one-line tick-ordering
bug. A faithful reimplementation needs the OpenGothic equivalents of the
original fields `oCNpc+0x7e4`/`+0x7e8` (pending vs current threat level),
`+0x7ec` (the decay timer) and the `+0x75c & 4` flag, plus the `NPC_ANGRY_TIME`
constant and the call sites that *raise* the level (the perception/assess
handlers that set `+0x7e4`). None of those fields or their writers exist in
OpenGothic today, so there is nothing to grep-verify against and no surgical,
build-verifiable edit to make. Implementing it would require first reversing the
producers of `+0x7e4` (the threat-raising perception paths) and adding new state
to `Npc`, which is out of scope for a single high-confidence tick fix.

## Conclusion

No not-yet-fixed, surgical, high-confidence divergence was found in the eight
requested tick mechanics — those paths read faithful or already carry fix-notes.
The only concrete gap is the entirely-absent `CheckAngryTime` per-frame decay,
recorded here as DEFERRED.
