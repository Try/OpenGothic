# Off-by-one / wrong-loop-bound sweep — NO FINDING

**Confidence:** N/A (NO FINDING)

## Summary

A focused sweep of the off-by-one / wrong-array-bound candidate surface listed in the
brief turned up no unhandled, high-confidence, build-verifiable divergence. Every concrete
candidate is either already corrected with an in-source parity `// NOTE:` or is provably
correct against the Daedalus/ZenKit array sizes. Per the "empty beats false positives" rule,
no patch is proposed.

## Areas examined (and why each is clean)

- **Damage-type loops** (`game/game/damagecalculator.cpp` swordDamage/rangeDamage/checkDamageMask,
  lines 135, 183, 192, 218, 246, 252, 266): all iterate `i < zenkit::DamageType::NUM` (== 8),
  matching `C_NPC.protection[8]` / `damage[8]`. Correct, and the file is densely annotated with
  verified parity NOTEs.
- **Protection array** (`game/game/inventory.cpp:892` applyArmor): `i < PROT_MAX` with
  `PROT_MAX == 8` (`game/game/constants.h:503`). Matches `protection[DamageType::NUM]`. Correct.
- **Talent arrays** (`world/objects/npc.cpp:1232-1254`, 4998): bounded by `TALENT_MAX_G2 == 22`
  with `talentsSk/talentsVl[TALENT_MAX_G2]`. Bounds are strict (`<`). Correct.
- **Hitchance array** (`world/objects/npc.cpp:1256-1264`): `t < zenkit::INpc::hitchance_count`
  (== 5) — the `<=`/strict-`<` off-by-one here was **already found and fixed** with a NOTE
  (the picklock stats-row out-of-bounds read). Excluded.
- **Perception** (`PERC_Count == 33`, `game/game/constants.h:442`): the array
  `perception[PERC_Count]` (`world/objects/npc.h:581`), the `PerDist::range[PERC_Count]` table
  (`game/game/gamescript.h:60`), and every accessor (`PerDist::at` @80, `perc_setrange` @3716,
  save/load @684/698, clears @628/3199) bound with strict `<` / `>=`. Correct.
- **Inventory spell slots** (`game/game/inventory.cpp` numslot loops @198, 702, 721, and the
  `id-3` / `slot-3` mapping for ids 3..10 -> `numslot[0..7]`, gated `3<=slot && slot<=10`):
  consistent with the noted 8-key design. Excluded (`numslot[8]` is the documented design choice).
- **Fight-AI move table** (`game/game/fightalgo.cpp:113` fillQueue): counts non-NOP entries up to
  `move_count == 6` then `rand(sz)` -> `0..sz-1`. Correct.
- **Interactive mob state machine** (`world/objects/interactive.cpp` isTrueDoor @605 `1..stateNum`,
  setAnim/animNpc clamps @1095/1156/1168, canQuitAtState @859): state range is `0..stateNum` and
  the clamps/loops are internally consistent with that and with the `S<n>`/`T_S<a>_2_S<b>` mob
  anim naming. No high-confidence divergence isolable without a full mob-anim decompile.
- **Cutscene camera arc-length** (`world/triggers/cscamera.cpp:18` `i<=100`): a 100-step Riemann
  integration of a cubic over [0,1]; the `<=` is the intended sample count. Correct.
- **Misc fixed-size arrays** (`aivar_count==100`, `condition_count==3`, `mission_count==5`,
  `NpcAttribute::NUM`, attribute HP/Mana clamps @npc.cpp:1308-1314): all iterated/bounded to the
  exact Daedalus sizes.

## Conclusion

The brief's enumerated hotspots have already been hardened (several carry explicit "off-by-one"
parity NOTEs, e.g. the hitchance picklock fix). No new surgical off-by-one with an observable
effect was found. **NO FINDING.**
