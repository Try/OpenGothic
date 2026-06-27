# Melee reach grants a phantom FIGHT_RANGE_1HA / 2HA guild bonus the engine ignores

**Confidence:** High (on the divergence); patch is a no-op in vanilla and a parity
correction for mods (see caveat).

## Original function + address (prose only)

The engine's unified melee reach / hit-connect predicate is
`oCNpc::IsInFightRange(zCVob*, float&)` at **0x0067cb60** (and its sibling
`oCNpc::IsInDoubleFightRange` at 0x0067c9a0). After computing the horizontal,
same-height, bbox-corrected gap to the target, it compares that gap against a
single threshold built from exactly two integers:

- the NPC's own *fight-range base* (object field at `+0x9d4`), and
- when a weapon is drawn, the **weapon item's own range** (`oCItem` field at
  `+0x26c`, i.e. the script `C_Item.range`); when unarmed, the NPC's
  *fight-range fist* (object field at `+0x9d8`).

So the per-weapon term is **purely the item's range** — there is no 1H/2H guild
bonus. `oCNpc::GetFightRange` (0x0067cd80) and `GetFightRangeDynamic`
(0x0067cdd0) confirm the same `base + item.range` formula.

Where do those NPC fields come from? `SetScriptValues` (writes around 0x006a5528)
reads the cached guild-value tables and loads **only** `FIGHT_RANGE_BASE` (table
at DAT_0x00aaeb20 → `SetFightRangeBase`, `+0x9d4`) and `FIGHT_RANGE_G`
(DAT_0x00aaed30 → `SetFightRangeG`, `+0x9dc`). `oCAniCtrl_Human::SetFightAnis`
(0x006aab48) loads `FIGHT_RANGE_FIST` (DAT_0x00aaec28 → `SetFightRangeFist`,
`+0x9d8`) **only** for the fist mode; for the 1H mode (param 3, table
DAT_0x00aaee38 = `FIGHT_RANGE_1HS`) and 2H mode (param 4, table DAT_0x00aaf048 =
`FIGHT_RANGE_2HS`) it reads the table value and then **discards it** — there is
no `SetFightRange*` call. The `FIGHT_RANGE_1HS/1HA/2HS/2HA` guild values are
therefore dead with respect to reach: the engine never adds them to the hit
threshold.

## OpenGothic file:line

`game/game/fightalgo.cpp:384-391` — `FightAlgo::weaponRange`. Used by the actual
hit-connect path via `isInWRange` (`game/world/objects/npc.cpp:54,1692,1837,1881`,
"need to be consistent with implAttack") and the attack AI.

## Divergence

```
case WeaponState::W1H: return float(gv.fight_range_1ha[gl] + add);   // add = item.range
case WeaponState::W2H: return float(gv.fight_range_2ha[gl] + add);
```

OpenGothic's weapon-state reach term for a drawn 1H/2H weapon is
`fight_range_1ha[guild] + item.range` (resp. `fight_range_2ha[...]`), whereas the
original engine's term is `item.range` alone. OG thus grants every drawn melee
weapon an extra `FIGHT_RANGE_1HA` / `FIGHT_RANGE_2HA` guild bonus that the engine
silently ignores, and it lets 1H and 2H reach differ where the original makes
them identical (item-range-driven). The function's own cited script comment —
`FAI_W = BASE + ItemRange (or Fist)` — already documents the correct formula
(`ItemRange`, *not* `1HA + ItemRange`); the fist case correctly uses
`fight_range_fist`, matching the "or Fist" branch, but the 1H/2H cases do not.

In vanilla G2 this is invisible because `FIGHT_RANGE_1HA/2HA` are ~0 (the in-code
empirical note "60 weapon range not enough, 70 good to hit on Bloofly" is only
consistent with the weapon term equalling the item range, i.e. the guild bonus
being 0). For mods that set these guild values non-zero, OG's reach diverges from
the original engine.

## Proposed patch

`add` is already `item.range` (`Item::swordLength()` returns `hitem->range`,
grep-verified at `game/world/objects/item.cpp:310-311`), which is exactly the
original's `oCItem+0x26c` term. Drop the phantom guild bonus and merge the two
cases (the original makes no 1H/2H distinction):

OLD (`game/game/fightalgo.cpp:384-388`):
```cpp
  switch(npc.weaponState()) {
    case WeaponState::W1H:
      return float(gv.fight_range_1ha[gl] + add);
    case WeaponState::W2H:
      return float(gv.fight_range_2ha[gl] + add);
```
NEW:
```cpp
  switch(npc.weaponState()) {
    case WeaponState::W1H:
    case WeaponState::W2H:
      // NOTE: in original-game oCNpc::IsInFightRange @0x0067cb60 (and GetFightRange
      // @0x0067cd80) the drawn-weapon reach term is the item's own range only
      // (oCItem field +0x26c == C_Item.range == swordLength()). FIGHT_RANGE_1HA/1HS/
      // 2HA/2HS are read by SetFightAnis @0x006aab48 but discarded, so they never
      // contribute to reach; only FIGHT_RANGE_FIST (fist) keeps a dedicated term below.
      return float(add);
```
(`gl`/`gv` remain used by the fist/other cases, so no unused-variable fallout.)

**Caveat / why not unconditional:** the magnitude depends on the mod's
`FIGHT_RANGE_1HA/2HA`, which I could not read out of `Gothic2.exe` (they live in
`Gothic.dat`). All evidence (engine decompile, the script-comment formula, and
OG's own Bloofly tuning note) indicates these are 0 in vanilla, making the patch a
strict no-op for vanilla and a parity fix for mods. If a maintainer can confirm
vanilla `FIGHT_RANGE_1HA == FIGHT_RANGE_2HA == 0`, this is a clean, build-verifiable
parity fix; treat as **DEFERRED** only if that confirmation is unavailable and the
empirically-tuned vanilla behavior must not be perturbed.
