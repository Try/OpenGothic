# AI_EquipBest* tie-break: original breaks equal-damage/protection ties by display name, OpenGothic uses item value

**Confidence:** Medium (divergence firmly established by decompilation; behavioral impact is low-frequency — it only changes which item is equipped when two usable candidates share an identical *full* damage/protection sum).

## Original function + address (prose)

`oCNpc::EquipBestWeapon` (@0x0074ef30) and `oCNpc::EquipBestArmor` (@0x0074f0b0) do **not** run a numeric "best" comparison of their own. Each calls `oCNpcInventory::UnpackCategory` (@0x0070f620) and then walks the category in display order via `oCNpcInventory::GetItem`, taking the **first** item that passes `oCNpc::CanUse` (@0x007319b0, the strength/dex condition gate) and the category-flag test (HasFlag 0x2 melee / 0x4 ranged + `IsMunitionAvailable`, or 0x10 armor); if that first item is already the active one (HasFlag 0x40000000) it returns without re-equipping, otherwise it calls `EquipWeapon`/`EquipArmor`.

"First in display order" is therefore decided entirely by the inventory display-sort comparator `FUN_00705b80` (P:\dev\g2addon\release\Gothic\_ulf\oInventory.cpp). Its relevant branches:
- **Weapons** (category-1 branch): sub-rank by weapon type, then `oCItem::GetFullDamage` (@0x00712500 — sum of the 8 `damage[]` entries) **descending**, then ties fall through to the universal tie-break `FUN_00705eb0`.
- **Armor** (category-2 branch): primary key the `wear` field at oCItem+0x18c ascending (constant = WEAR_TORSO for all body armor in G2), then `oCItem::GetFullProtection` (@0x00712340 — sum of the 8 `protection[]` entries) **descending**, then the same tie-break.
- `FUN_00705eb0` compares the two items' display text (`oCItem::GetText`, vtbl+0x7c) and orders **alphabetically by name, ascending**.

Crucially, neither the weapon nor the armor branch ever keys on item **value/cost** — the only tie-break after the damage/protection sum is the display name.

## OpenGothic file:line

`game/game/inventory.cpp:1052` — `Inventory::bestItem` (used by `bestMeleeWeapon`/`bestRangedWeapon`/`bestArmor`, lines 1088–1098), specifically the comparison at line 1079:

```cpp
    if(std::make_tuple(key, itData.value)>std::make_tuple(damage, value)){
      ret    = i.get();
      damage = key;
      value  = itData.value;
      }
```

## Divergence

The primary key (`key` = sum of `damage[]`/`protection[]`, descending) already matches the original (and matches `GetFullDamage`/`GetFullProtection` since `zenkit::DamageType::NUM == 8`). The **tie-break diverges**: OpenGothic breaks an equal-`key` tie by `itData.value` (cost), preferring the **most expensive** candidate, whereas the original game has no value key at all and breaks the tie by **display name, ascending** (`FUN_00705eb0`). When an NPC holds two usable weapons of identical full damage (or two armors of identical full protection), the original equips the alphabetically-first one; OpenGothic equips the costlier one. The `value` key is a fabricated ranking dimension not present in the original comparator.

## Proposed patch

Replace the value tie-break with a display-name-ascending tie-break (and drop the now-unused `value` local at lines 1054 and 1082). `Item::displayName()` exists (`game/world/objects/item.h:54`, already used by `Inventory::less` at line 1146).

OLD (inventory.cpp:1079–1083):
```cpp
    if(std::make_tuple(key, itData.value)>std::make_tuple(damage, value)){
      ret    = i.get();
      damage = key;
      value  = itData.value;
      }
```

NEW:
```cpp
    // NOTE: in original-game inventory display-sort comparator @0x00705B80 (which oCNpc::EquipBestWeapon
    // @0x0074ef30 / EquipBestArmor @0x0074f0b0 walk via GetItem to take the first usable item) the
    // weapon branch keys on oCItem::GetFullDamage @0x00712500 and the armor branch on
    // oCItem::GetFullProtection @0x00712340, both descending, then ties break on display name
    // (oCItem::GetText) ascending @0x00705eb0 -- there is no value/cost key. Mirror the name tie-break.
    if(ret==nullptr || key>damage || (key==damage && i->displayName()<ret->displayName())){
      ret    = i.get();
      damage = key;
      }
```

Also remove `int32_t value = std::numeric_limits<int32_t>::min();` (line 1054), which becomes unused.

Note: the same fabricated value/cost key also lives in the display-sort comparator `Inventory::less` (line 1146, `-lV`); the existing NOTE there already flags the name tie-break. Aligning `bestItem` is the surgical, EquipBest-scoped fix; reconciling `less` is out of scope here.
