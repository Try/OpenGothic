# FAI: no sidestep (STRAFE) when an NPC blocks the approach path

> DEFER: needs a new IsNpcBetweenMeAndTarget raycast helper (non-surgical) + runtime tuning.

**Confidence:** Medium

## Original function

`oCNpc::FindNextFightAction` (Gothic2.exe `0x0067d680`, oNpc_Fight.cpp).
Immediately before the FAI situation switch, for melee weapon modes, there is
an early return:

- `local_b8` = target is inside G-range (distance < base+G+weapon-range).
- `local_b4` = target is inside W-range (melee reach).
- guard: `local_b8 != 0 && local_b4 == 0 && IsNpcBetweenMeAndTarget(me,target)`
  -> `return STRAFE` (move code 5).

Meaning: while closing in (target within walking G-range but not yet within
melee W-range), if another NPC is standing between me and the target, the
fighter strafes sideways to step around the obstruction instead of running
straight into it.

## OpenGothic

`game/game/fightalgo.cpp` -- `fillQueue(npc,tg,owner)` has no equivalent of
`IsNpcBetweenMeAndTarget`, and no STRAFE-to-sidestep branch. Closing-in NPCs
queue runto/move and walk straight at the target regardless of an ally
blocking the lane.

## Divergence

A melee NPC whose path to the target is blocked by another NPC will, in the
original, sidestep (STRAFE) around it; in OG it does not, so it bunches up /
pushes into the blocking ally during group fights. Gameplay-different, but
isolated to the obstructed-approach case.

## Proposed patch

Not a one-line value flip: OpenGothic has no `IsNpcBetweenMeAndTarget`
equivalent, so a faithful fix needs a new helper (a short forward
raycast/collision check from the NPC toward the target that reports an
intervening NPC), then, in `fillQueue` before the weapon-state branches:

```
// NOTE: in original-game oCNpc::FindNextFightAction (0x0067d680): when the
// target is in G-range but not yet in W-range and another NPC blocks the
// path, the fighter returns STRAFE to sidestep around it.
if(isInGRange(npc,tg,owner) && !isInWRange(npc,tg,owner) && isNpcBetween(npc,tg)) {
  queueId = zenkit::FightAiMove::STRAFE;
  return;
  }
```

Documented for completeness; defer until the `isNpcBetween` helper exists.
