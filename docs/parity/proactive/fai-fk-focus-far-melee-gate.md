# FAI parity: `my_fk_focus_far` gated on melee W-range instead of the long FK aim range

**Confidence:** High

## Original function + address

`oCNpc::FindNextFightAction` @ `0x0067d680` (with helper `oCNpc::IsInFightFocus`
@ `0x00735290` and `oCNpc::IsInFightRange`).

For a fighter that is neither in melee weapon-mode-and-range nor a caster, the original
falls through to its two lowest-priority "FK far" (long-range / ranged) tables. In the
priority chain these are the last two cases, selecting the script tables `my_fk_focus_far`
(struct offset `0x16c`) and `my_fk_nofocus_far` (struct offset `0x188`). Their gates are:

- `my_fk_focus_far`  : `in_FK_range AND fight_focus`
- `my_fk_nofocus_far`: `in_FK_range`

`in_FK_range` is `horizontal_distance < 3000.0` units (the constant `0x453b8000` = 3000.0,
i.e. the 30 m "FK" long-combat range), computed from the same horizontal `dx^2+dz^2` distance
`IsInFightRange` writes out, and is independent of the same-height test. `fight_focus` is
`IsInFightFocus(self,target)` — the aiming cone (abs(yaw) < 30, abs(pitch) < 50, within range).
Crucially the focused table's range gate is the *long* FK range, NOT the melee weapon range.

## OpenGothic file:line

`game/game/fightalgo.cpp:98-101`

```
  if(isInWRange(npc,tg,owner) && focus)
    if(fillQueue(owner,ai.my_fk_focus_far))
      return;
  if(fillQueue(owner,ai.my_fk_nofocus_far))
    return;
```

## Divergence

OpenGothic gates `my_fk_focus_far` on `isInWRange(npc,tg,owner)` — the *melee* preferred-attack
distance (`base + base + weaponRange`, i.e. a few weapon-lengths). A bow/crossbow fighter (and
any ranged combatant) engages from well outside melee range, so `isInWRange` is false and
`my_fk_focus_far` is effectively unreachable: the NPC always drops to `my_fk_nofocus_far` even
when it is correctly aimed at its target. The original instead reaches `my_fk_focus_far` whenever
the target is within the long FK range and inside the aim cone, regardless of melee proximity.

This is the exact same class of bug already fixed for the magic branch (`my_fk_focus_mag` was
gated on melee weapon-range instead of the aim cone), applied here to the ranged/bow FK-far
branch — a different table and a different code path, so it is distinct from that fix.

## Proposed patch

Mirror the accepted magic-branch fix: drop the melee W-range requirement and gate the focused
FK-far table on the aim/focus cone (the `focus` flag), matching the original which never gates
this table on melee weapon-range. (`my_fk_nofocus_far` is already the unconditional fall-through
immediately after, mirroring the original's `in_FK_range`-only nofocus gate.)

```
OLD:
  if(isInWRange(npc,tg,owner) && focus)
    if(fillQueue(owner,ai.my_fk_focus_far))
      return;
  if(fillQueue(owner,ai.my_fk_nofocus_far))
    return;

NEW:
  // NOTE: in original-game oCNpc::FindNextFightAction @0x0067d680 the focused long-range
  // table my_fk_focus_far is gated on the FK aim range (horizontal dist < 3000u, the 30m
  // "FK" range) AND IsInFightFocus (@0x00735290, the aim cone) -- NOT on melee weapon-range.
  // A bow/crossbow fighter engages from outside melee range, so the original isInWRange gate
  // made my_fk_focus_far unreachable and ranged NPCs always fell through to my_fk_nofocus_far
  // even while properly aimed. Same class as the already-fixed my_fk_focus_mag gate.
  if(focus)
    if(fillQueue(owner,ai.my_fk_focus_far))
      return;
  if(fillQueue(owner,ai.my_fk_nofocus_far))
    return;
```

Grep-verified OG symbols: `focus` (local bool, fightalgo.cpp:51), `ai.my_fk_focus_far` /
`ai.my_fk_nofocus_far` (`FightAi::FA`, fightaidefinitions.h), `fillQueue(GameScript&, const
zenkit::IFightAi&)`.

Note: a stricter faithful variant would gate on `focus` AND a horizontal FK-range check
(`d = npc.fightDistanceTo(tg); d.x*d.x+d.z*d.z < 3000.f*3000.f`, deliberately NOT `qDistTo`,
which folds in the melee same-height test that the original FK range does not apply). The
`focus`-only form is preferred here for parity with the previously accepted magic-branch fix
and minimal risk, since `my_fk_nofocus_far` is already reached unconditionally.
