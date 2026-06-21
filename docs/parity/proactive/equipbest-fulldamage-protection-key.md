# Auto-equip best item: sort key (damage_total/value) vs original full-damage/full-protection sum

**Confidence:** Medium

## Original function + address

`oCNpc::EquipBestWeapon` (0x0074ef30) and `oCNpc::EquipBestArmor` (0x0074f0b0)
do NOT compute a max themselves. They call `UnpackCategory`, then walk the
inventory in its already-sorted display order and equip the FIRST usable item
(`CanUse` true, correct category flag, ammo available for ranged, and not flag
0x40000000). The inventory list is kept sorted by the list comparator at
0x00705b80 (set as the list-sort fn in the oCNpcInventory ctor at 0x0070baf0).

That comparator, for the weapon/ammo category bucket, orders items primarily by
`oCItem::GetFullDamage` (0x00712500) descending, and for the armor bucket by the
`wear` field (which body region the armor covers, offset 0x18c) then
`oCItem::GetFullProtection` (0x00712340) descending; ties fall through to an
alphabetical item-name compare (0x00705eb0). For body armor `wear` is constant,
so the effective armor key is summed protection.

Crucially:
- `GetFullDamage` returns the SUM of the eight per-type `damage[]` values
  (offset 0x16c), NOT the scalar `damage_total` script field (offset 0x168).
  `oCItem::ApplyDamages` (0x0065e5a0) distributes `damage_total` across the set
  damage types only when a per-type slot was left at 0, so for weapons whose
  script fills `damage[]` directly the summed array and `damage_total` differ.
- `GetFullProtection` returns the SUM of the eight per-type `protection[]`
  values; the original armor key is protection, never resale value.

So "first usable in sorted order" reduces to: highest summed per-type damage
(weapons) / highest summed per-type protection (armor).

## OpenGothic file:line

`game/game/inventory.cpp:1019` (`Inventory::bestItem`)

```cpp
if(std::make_tuple(itData.damage_total, itData.value)>std::make_tuple(damage, value)){
```

`bestArmor`, `bestMeleeWeapon`, `bestRangedWeapon` all route through this.

## Divergence

- Weapons: OG ranks by the scalar `damage_total` field; original ranks by
  `sum(damage[])`. They match for the common idiom (single damage type set via
  `damage_total`) but differ for weapons that populate the per-type `damage[]`
  array directly (`damage_total` left 0 / set to a headline number that is not
  the array sum). OG can then equip a different weapon.
- Armor: armor has `damage_total==0`, so OG's `(damage_total,value)` key ranks
  armor purely by `value` (cost). The original ranks armor by summed
  protection. Whenever an NPC carries two usable armors where the costlier one
  is not the better-protecting one, OG equips a different piece.
- Tie-break: original uses item NAME (alphabetical); OG uses `value`. (Listed
  for completeness; on its own this is Low.)

## Proposed patch

```cpp
// game/game/inventory.cpp  (Inventory::bestItem)
// OLD
Item* Inventory::bestItem(Npc &owner, ItmFlags f) {
  Item*   ret    = nullptr;
  int32_t value  = std::numeric_limits<int32_t>::min();
  int32_t damage = std::numeric_limits<int32_t>::min();
  for(auto& i:items) {
    auto& itData = i->handle();
    auto  flag   = ItmFlags(itData.main_flag);
    if((flag & f)==0)
      continue;
    if(!i->checkCond(owner))
      continue;
    if(itData.munition>0 && findByClass(size_t(itData.munition))==nullptr)
      continue;

    if(std::make_tuple(itData.damage_total, itData.value)>std::make_tuple(damage, value)){
      ret    = i.get();
      damage = itData.damage_total;
      value  = itData.value;
      }
    }
  return ret;
  }

// NEW
Item* Inventory::bestItem(Npc &owner, ItmFlags f) {
  Item*   ret    = nullptr;
  int32_t value  = std::numeric_limits<int32_t>::min();
  int32_t damage = std::numeric_limits<int32_t>::min();
  for(auto& i:items) {
    auto& itData = i->handle();
    auto  flag   = ItmFlags(itData.main_flag);
    if((flag & f)==0)
      continue;
    if(!i->checkCond(owner))
      continue;
    if(itData.munition>0 && findByClass(size_t(itData.munition))==nullptr)
      continue;

    // NOTE: in original-game oCNpc::EquipBestWeapon/EquipBestArmor pick the first
    // usable item in inventory display order; that order ranks weapons by
    // oCItem::GetFullDamage (sum of the per-type damage[] array, not the scalar
    // damage_total field) and armor by oCItem::GetFullProtection (sum of the
    // per-type protection[] array), descending. Mirror that summed key here.
    int32_t key = 0;
    if(flag & ITM_CAT_ARMOR) {
      for(size_t d=0; d<zenkit::DamageType::NUM; ++d)
        key += itData.protection[d];
      } else {
      for(size_t d=0; d<zenkit::DamageType::NUM; ++d)
        key += itData.damage[d];
      }

    if(std::make_tuple(key, itData.value)>std::make_tuple(damage, value)){
      ret    = i.get();
      damage = key;
      value  = itData.value;
      }
    }
  return ret;
  }
```

Note: this preserves OG's `value` tie-break rather than the original's
name tie-break (name tie-break alone is Low and would need a separate change).
The summed-array key is the load-bearing parity fix.
