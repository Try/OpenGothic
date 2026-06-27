# Weapon `damage_total` is multiplied across damage types instead of split evenly

**Confidence:** Medium-High. The original `ApplyDamages` logic is unambiguous in the
decompile; the divergence is a clear logic mismatch. Observability in *pure vanilla*
Gothic 2 is limited because almost every player weapon has a single damage-type bit
(where the two formulas coincide), so this primarily affects multi-damage-type weapons
and any weapon that sets both a per-type `damage[]` entry and a scalar `damage_total`
(common in mods / a true engine-parity gap).

## Original function + address (prose only)

`oCItem::InitByScript` (Gothic2.exe `0x00711bd0`) finalizes an item's per-type damage
array by calling the helper `ApplyDamages` (oDamage.cpp, Gothic2.exe `0x0065e5a0`),
passing the item's `damage_type` mask, the per-type `damage[8]` array, and the scalar
`damage_total`.

`ApplyDamages(mask, damage[8], &damageTotal)` behaves as follows:
1. It counts how many damage-type bits are set in `mask` (one `+1.0` per set bit, over
   the 8 type bits) into a float `n`.
2. If `n != 0` it computes `spread = (int)(damageTotal / n)` — `damage_total` divided
   **evenly** across the set damage types (integer-truncated via `__ftol`).
3. For each bit `i` that is set in `mask`, **only if** the existing per-type
   `damage[i] == 0`, it writes `damage[i] = spread`. A per-type slot that was already
   non-zero (explicitly authored) is **left untouched**, and `damage_total` is *not*
   added to it.

So after init, a weapon's effective per-type damage is: for each set type, its own
`damage[i]` if non-zero, otherwise `damageTotal / numSetTypes`; the scalar total is
spread, never multiplied.

## OpenGothic file:line

`game/game/inventory.cpp:1131` — `Inventory::applyWeaponStats(Npc&, const Item&, int sgn)`
(folds the equipped weapon's effective damage into `hnpc.damage[]` on equip/unequip,
which is OpenGothic's stand-in for the original's per-hit `oCItem` damage lookup).

## Divergence

Current OG (lines 1134-1139):

```cpp
for(size_t i=0; i<zenkit::DamageType::NUM; ++i){
  hnpc.damage[i] += sgn*weapon.handle().damage[i];
  if(weapon.handle().damage_type & (1<<i)) {
    hnpc.damage[i] += sgn*weapon.handle().damage_total;
    }
  }
```

This adds the **full** `damage_total` to **every** set type, and adds it **on top of**
a non-zero per-type `damage[i]`. Versus the original `ApplyDamages`:

* Multi-type weapon (e.g. `damage_type = EDGE|FIRE`, `damage[]` all 0,
  `damage_total = 100`): original yields EDGE=50, FIRE=50 (total 100); OG yields
  EDGE=100, FIRE=100 (total 200) — an `n`× over-count.
* Weapon with an explicit per-type value and a scalar total (e.g. `damage_type=EDGE`,
  `damage[EDGE]=80`, `damage_total=100`): original keeps EDGE=80 (ignores the scalar);
  OG yields EDGE=180.

Single-bit, `damage[i]==0` weapons (the vanilla norm) match in both, which is why the
common case is unaffected. ZenKit exposes the raw script fields (`daedalus.hh:280-282`:
`damage_type`, `damage_total`, `damage[DamageType::NUM]`), it does not run the engine's
`ApplyDamages` spread, so OpenGothic must replicate it here.

## Proposed patch

OLD (`game/game/inventory.cpp`, in `applyWeaponStats`):
```cpp
  auto& hnpc = owner.handle();
  //hnpc.damagetype = sgn>0 ? weapon.handle()->damageType : (1 << GEngineClasses::DAM_INDEX_BLUNT);
  for(size_t i=0; i<zenkit::DamageType::NUM; ++i){
    hnpc.damage[i] += sgn*weapon.handle().damage[i];
    if(weapon.handle().damage_type & (1<<i)) {
      hnpc.damage[i] += sgn*weapon.handle().damage_total;
      }
    }
```

NEW:
```cpp
  auto& hnpc = owner.handle();
  auto& h    = weapon.handle();
  //hnpc.damagetype = sgn>0 ? weapon.handle()->damageType : (1 << GEngineClasses::DAM_INDEX_BLUNT);
  // NOTE: in original-game ApplyDamages (Gothic2.exe 0x0065e5a0, called from
  // oCItem::InitByScript @0x00711bd0) the scalar damage_total is spread EVENLY across the
  // set damage types: each set type gets damage_total/numSetTypes, and only if its per-type
  // damage[i] is still 0 (an authored per-type value is kept, and damage_total is not added
  // on top of it). Adding the full damage_total to every set type multiplied multi-type
  // weapon damage by the number of types and double-counted explicit per-type values.
  int numTypes = 0;
  for(size_t i=0; i<zenkit::DamageType::NUM; ++i)
    if(h.damage_type & (1<<i))
      ++numTypes;
  const int32_t spread = numTypes>0 ? h.damage_total/numTypes : 0;
  for(size_t i=0; i<zenkit::DamageType::NUM; ++i){
    int32_t d = h.damage[i];
    if((h.damage_type & (1<<i)) && d==0)
      d = spread;
    hnpc.damage[i] += sgn*d;
    }
```

Symmetric under `sgn` (the computed contribution is a pure function of the constant
weapon fields), so equip/unequip still cancels exactly. Grep-verified symbols:
`zenkit::DamageType::NUM` (daedalus.hh:70), `weapon.handle().damage` /
`.damage_total` / `.damage_type` (daedalus.hh:280-282, already used at the OLD site),
`owner.handle()` (existing).
