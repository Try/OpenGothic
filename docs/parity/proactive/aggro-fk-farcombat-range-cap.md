# Aggression / fight-band: missing FK (far-combat) 30 m range cap on melee fighters

**Confidence:** Medium (divergence is a verified hardcoded constant present in the
original and absent in OpenGothic; behavioral impact is marginal, see below).

**Status:** DEFERRED (reason at the end).

## Original function + address

`oCNpc::FindNextFightAction` (Gothic2.exe @0x0067d680, `oNpc_Fight.cpp`) is the
per-tick fight-move selector. For a **melee** weapon mode (`GetWeaponMode() < 5`,
i.e. fist / 1H / 2H) it computes three nested distance bands against the target
and gates the candidate fight-move lists on them:

- **W band** (`local_b4` / `local_8c`): `oCNpc::IsInFightRange` — the true weapon
  reach, horizontal `sqrt(dx*dx+dz*dz)` distance, bbox-trimmed, and gated on
  `IsSameHeight`. Compared to `fight_range_base + weaponReach`.
- **G band** (`local_b8`): the same center distance compared to
  `fight_range_base + weaponReach + fight_range_g` (a wider buffer band).
- **FK band** (`local_98`): the same center distance compared against a *hardcoded
  literal* `3000.0` (float constant `___real_453b8000` == 3000.0 == 30 m). The
  far-combat move lists (the `my_fk_*` bands, switch cases 0x0D/0x0E that read the
  fight-AI move arrays at struct offsets `+0x16c` / `+0x188`) only fire when
  `dist < 3000`. When the target is farther than 3000 units the loop never sets a
  move and the function falls through to its weapon-/tactic-default move
  (the `(-(tactic!=1)&0xfffffffd)+4` => MV 1 or 4 path) — it does **not** issue an
  `my_fk_*` move.

(For bow/crossbow weapon mode 5/6 and mage mode 7 the original takes the separate
`+0x1a4/+0x1c0` and `+0x1dc/+0x1f8` focus/nofocus branches with no distance gate,
so the 3000 cap is specific to the melee path.)

## OpenGothic file:line

`game/game/fightalgo.cpp:94-97` (`FightAlgo::fillQueue`):

```cpp
  if(isInWRange(npc,tg,owner) && focus)
    if(fillQueue(owner,ai.my_fk_focus_far))
      return;
  if(fillQueue(owner,ai.my_fk_nofocus_far))
    return;
```

`isInWRange`/`isInGRange` are defined at `game/game/fightalgo.cpp:327-341`;
`my_fk_focus_far` / `my_fk_nofocus_far` are grep-verified zenkit `IFightAi` fields
(also referenced at fightalgo.cpp:88-90 for the mage variants).

## Divergence

OpenGothic's `fillQueue` reaches the `my_fk_*` far-combat bands as the last
fallback after the W and G bands fail (i.e. for a *distant* target), and fires
`my_fk_nofocus_far` **unconditionally at any distance** — there is no 3000-unit
(30 m) cap. In the original, a melee NPC whose target is beyond 30 m does **not**
emit an `my_fk_*` move; it returns the tactic-default move instead. So at very long
range OpenGothic keeps selecting the scripted far-combat move list where the
original would have selected the default.

## Why marginal / DEFERRED

- The `my_fk_*` lists for melee tactics in the vanilla G2 fight-AI tables are
  approach/"RUNTO"-style moves, and OpenGothic's tactic-default tail
  (`fillQueue(owner,ai.my_w_nofocus)` at fightalgo.cpp:100) is also an approach.
  At >30 m both engines effectively make the NPC close the distance, so the visible
  behavior is approach-vs-approach.
- OpenGothic's `FightAlgo::fillQueue` is a deliberate heuristic re-implementation
  with a different control structure than `FindNextFightAction` (it does not mirror
  the 15-case switch). A surgical, build-verifiable 1:1 graft of the `dist<3000`
  literal cannot be placed without first reconciling that structural difference,
  which risks regressing the already-tuned W/G band selection. This is adjacent to
  the already-deferred `fai-grange` item.
- No script symbol backs the 3000 literal (it is a raw engine constant), so there
  is no grep-verifiable OG symbol to cite for a clean-room re-expression beyond a
  new named constant.

A defensible future fix (NOT applied) would be: only enter the `my_fk_*` far bands
for melee weapon states when `qDistTo(npc,tg) < 3000*3000`, otherwise fall through
to `my_w_nofocus`. Deferred pending confirmation that it produces an observable
difference and does not disturb ranged/mage paths.

```
// NOTE: in original-game oCNpc::FindNextFightAction @0x0067d680 the melee far-combat
// (my_fk_*) move bands are gated on dist < 3000.0 (30 m); beyond that the original
// returns the tactic-default move instead of an my_fk_* move.
```
