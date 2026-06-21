# Item change_atr / change_value attribute bonuses are never applied

**Confidence:** Medium

## Original function + address

`oCNpc::AddItemEffects` (Gothic2.exe `0x007320f0`) and its mirror
`oCNpc::RemoveItemEffects` (`0x00732270`) are invoked when an item is equipped /
unequipped (called from `EquipItem` `0x007323c0`).

Besides applying the 8 `protection[]` indices (add on equip, subtract on
unequip), each of these functions iterates the item's 3-element
`change_atr[]` / `change_value[]` arrays. For every entry whose `change_atr`
index is `> 0` it calls the engine attribute-change routine (`0x0072ff60`,
i.e. `oCNpc::ChangeAttribute`) with `+change_value` on equip and
`-change_value` on unequip. That routine adds the value to
`attribute[change_atr]`, clamps the result to `>= 0`, and — for
`ATR_HITPOINTS`/`ATR_MANA` — clamps the current value down to its
`*_MAX` counterpart. (Offsets confirmed: `cond_atr` at item+0x1b4 from
`CanUse` `0x007319b0`; `change_atr` at item+0x1cc, `change_value` at
item+0x1d8 — the array pair immediately after `cond_*`.)

Net effect in the original: equipping a ring/amulet/belt/armor that defines
`change_atr`/`change_value` grants the corresponding attribute bonus
(e.g. +strength, +mana-max), and unequipping removes it symmetrically.

## OpenGothic divergence

`game/world/objects/item.cpp:52,122` read and write `change_atr` /
`change_value` into the item handle, but **no code path ever applies them**.
`Inventory::setSlot` (`game/game/inventory.cpp:402-470`) applies only
`applyArmor` (the `protection[]` array, `inventory.cpp:858-862`) and
`applyWeaponStats` (weapon damage only, `inventory.cpp:1061`). A grep for
`change_atr` / `change_value` across `game/` shows only the serialization
reads. Items relying on the engine `change_atr` mechanism therefore grant no
bonus in OpenGothic.

Rated Medium (not High) because vanilla Gothic 1/2 content overwhelmingly
uses `on_equip`/`on_unequip` scripts rather than `change_atr`; entries left at
0 are a no-op in the original too (`change_value != 0` guard). But any item /
mod that populates `change_atr` diverges, and the engine mechanism is missing
entirely.

## Proposed patch

`game/game/inventory.cpp` — `Inventory::applyArmor` is already the per-slot
"apply with sign" helper invoked on both equip (+1) and unequip (-1) for every
slot type, mirroring AddItemEffects/RemoveItemEffects. Extend it:

OLD:
```cpp
void Inventory::applyArmor(Item &it, Npc &owner, int32_t sgn) {
  for(size_t i=0;i<PROT_MAX;++i){
    auto v = owner.protection(Protection(i));
    owner.changeProtection(Protection(i),v+it.handle().protection[i]*sgn);
    }
  }
```

NEW:
```cpp
void Inventory::applyArmor(Item &it, Npc &owner, int32_t sgn) {
  for(size_t i=0;i<PROT_MAX;++i){
    auto v = owner.protection(Protection(i));
    owner.changeProtection(Protection(i),v+it.handle().protection[i]*sgn);
    }
  // NOTE: in original-game oCNpc::AddItemEffects (Gothic2.exe 0x007320f0) /
  // RemoveItemEffects (0x00732270) each equipped item also applies its
  // change_atr[]/change_value[] pairs as attribute bonuses (add on equip,
  // subtract on unequip). Entries with change_atr index <= 0 are skipped.
  for(size_t i=0;i<zenkit::IItem::condition_count;++i){
    const int32_t atr = it.handle().change_atr[i];
    if(atr>0)
      owner.changeAttribute(Attribute(atr), it.handle().change_value[i]*sgn, false);
    }
  }
```

`Npc::changeAttribute` (`npc.cpp:1244`) already replicates `0x0072ff60`'s
clamp-to-0 and HP/MANA-vs-MAX clamping, so the apply path matches. The
`atr>0` guard mirrors the original's `0 < change_atr` test.
