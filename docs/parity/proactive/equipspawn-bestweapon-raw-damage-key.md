# Equip-on-spawn: best-weapon selection ranks by raw (unspread) per-type damage, scoring every standard weapon as 0

**Confidence:** High

## Original function + address

On NPC creation the script-set damage scalar is baked into the per-type array by
`oCItem::InitByScript` (Gothic2.exe @0x00711bd0), which calls `ApplyDamages` (@0x0065e5a0):
the scalar `damageTotal` is spread evenly across the *set* damage types (`damageTotal /
numSetTypes`) and written into each typed per-type slot (item+0x16c) that is still 0. So by the
time an item sits in inventory, its `damage[8]` array holds the spread values.

The auto-equip-best path ranks candidates by inventory display order. `oCNpc::EquipBestWeapon`
(@0x0074ef30) walks the unpacked weapon category and takes the first usable item; that order is
produced by the inventory sort comparator (@0x00705B80), whose weapon branch (case 1) compares
`oCItem::GetFullDamage(a)` vs `GetFullDamage(b)` **descending**. `oCItem::GetFullDamage`
(@0x00712500) returns the **sum of the 8-element per-type `damage[]` array** (item+0x16c) — i.e.
the *spread* values, which for a normal weapon equal `damageTotal`. (The armor branch, case 2,
analogously sums `oCItem::GetFullProtection` @0x007125-region over `protection[8]` at item+0x190.)

## OpenGothic file:line

`/Users/admin/Downloads/opengothic/game/game/inventory.cpp:1094-1131` — `Inventory::bestItem`,
specifically the weapon-key block at lines 1112-1119. This is the on-spawn auto-equip selection:
`Npc::Npc`/`Npc::resetPositionToTA` → `Inventory::autoEquipWeapons` →
`equipBestMeleeWeapon`/`equipBestRangedWeapon` → `bestMeleeWeapon`/`bestRangedWeapon` →
`bestItem`.

## Divergence

`bestItem` builds its ranking key for a weapon by summing the **raw** zenkit handle
`itData.damage[d]` array:

```cpp
for(size_t d=0; d<zenkit::DamageType::NUM; ++d)
  key += itData.damage[d];
```

OpenGothic never spreads `damage_total` into the handle's `damage[]` array — it performs the
spread only at equip time, into the NPC's `damage[]` accumulator (see `applyWeaponStats`,
inventory.cpp:1149-1166, which reads `h.damage[i]` as the *raw, unspread* authored value and
substitutes `damage_total/numTypes` where a typed slot is 0). Standard Gothic II weapon instances
author only `damageTotal` (+ `damagetype`) and leave the `damage[]` array at 0. Therefore the sum
of `itData.damage[]` is **0 for essentially every melee/ranged weapon**, so every weapon candidate
gets `key == 0` and the "best" weapon is decided purely by the alphabetical `displayName()`
tie-break (inventory.cpp:1125) — not by damage.

Result: on spawn an NPC auto-equips the alphabetically-first weapon it carries instead of its
strongest one (e.g. a guard holding a rusty sword + a heavy two-hander draws whichever sorts first
by name). The original ranks by `GetFullDamage`, i.e. the effective total damage, and equips the
strongest. (The armor branch is unaffected: armor scripts author the `protection[]` array
explicitly, so summing it already matches `GetFullProtection`.)

## Proposed patch

Mirror the engine spread (identical to `applyWeaponStats`) when building the weapon key, so the
key equals the original's `GetFullDamage` over the spread array. Armor branch unchanged.

OLD (inventory.cpp:1112-1119):
```cpp
    int32_t key = 0;
    if(flag & ITM_CAT_ARMOR) {
      for(size_t d=0; d<zenkit::DamageType::NUM; ++d)
        key += itData.protection[d];
      } else {
      for(size_t d=0; d<zenkit::DamageType::NUM; ++d)
        key += itData.damage[d];
      }
```

NEW:
```cpp
    int32_t key = 0;
    if(flag & ITM_CAT_ARMOR) {
      for(size_t d=0; d<zenkit::DamageType::NUM; ++d)
        key += itData.protection[d];
      } else {
      // NOTE: in original-game oCItem::InitByScript @0x00711bd0 -> ApplyDamages @0x0065e5a0 spreads
      // the scalar damage_total evenly across the set damage types (damage_total/numSetTypes) into
      // each typed per-type slot still 0 *before* the item enters inventory. The equip-best path
      // (oCNpc::EquipBestWeapon @0x0074ef30, walking the inventory sort comparator @0x00705B80) ranks
      // weapons by oCItem::GetFullDamage @0x00712500 = sum of the *spread* damage[] array. Standard
      // weapons author only damageTotal and leave damage[] zero, so summing the raw handle array gave
      // key==0 for every weapon, reducing selection to the alphabetical name tie-break. Reproduce the
      // spread here (as applyWeaponStats already does) so the strongest weapon is chosen.
      int numTypes = 0;
      for(size_t d=0; d<zenkit::DamageType::NUM; ++d)
        if(itData.damage_type & (1<<d))
          ++numTypes;
      const int32_t spread = numTypes>0 ? itData.damage_total/numTypes : 0;
      for(size_t d=0; d<zenkit::DamageType::NUM; ++d) {
        int32_t dmg = itData.damage[d];
        if((itData.damage_type & (1<<d)) && dmg==0)
          dmg = spread;
        key += dmg;
        }
      }
```

Grep-verified symbols: `itData.damage_type`, `itData.damage_total`, `itData.damage[]`,
`itData.protection[]` (inventory.cpp:1109-1164, item.cpp:51); `zenkit::DamageType::NUM`
(inventory.cpp:1114,1156). The spread idiom is copied 1:1 from `applyWeaponStats`
(inventory.cpp:1155-1164).
