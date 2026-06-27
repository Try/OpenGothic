# use2: Item effects (protection / change_atr / disguise_guild) not applied on the FOOD-consume path

**Confidence:** Medium (divergence is decompiler-proven and grep-verified; in-game visibility is low because vanilla
Gothic 2 consumables rarely carry these fields — the fix is a faithful, side-effect-free parity restore).

## Original function + address

`oCNpc::UseItem` (Gothic2.exe `0x0073bc10`). After the `CanUse` gate, the function tests the runtime flags
field with `oCItem::HasFlag(0x20)` (`0x007126d0`, which AND-tests the item's flag word at offset `0x158`).
`0x20` is the FOOD/consumable runtime bit (the `ITEM_KAT_FOOD` mainflag folded into the runtime flags). Inside
that FOOD branch the engine performs, in order:

1. `ChangeAttribute(ATR_HITPOINTS, nutrition)` (`0x0072ff60`) — the nutrition heal (already ported, see below).
2. **`AddItemEffects(this, item)` (`0x007320f0`, called from `UseItem` at `0x0073bd0a`)** — applies the item's
   on-effect bonuses to the consumer: per-index `protection[]` is summed into the NPC's protection, each
   positive `change_atr[i]` adds `change_value[i]` to the corresponding attribute, a non-zero `disguise_guild`
   (differing from the true guild) overwrites the live guild, and the item's effect script function is invoked
   with `SELF` bound. Because the consumed item is then removed, there is no paired `RemoveItemEffects`, so the
   bonus is permanent — this is the engine mechanism for a consumable that grants a lasting protection/attribute
   change.
3. Decrement the stack / remove the last unit.

`AddItemEffects`'s xref set confirms it is shared by both the equip paths (`Equip @0x0073a003`,
`EquipItem @0x0073246e`) **and** the consume path (`UseItem @0x0073bd0a`).

## OpenGothic file:line

`game/game/inventory.cpp` — `Inventory::use()`, lines 1008–1014 (the FOOD-consume branch).

OpenGothic's equivalent of `AddItemEffects` is `Inventory::applyArmor()` (inventory.cpp:891), which already
mirrors the protection sum, the `change_atr[]/change_value[]` bonuses, and the `disguise_guild` overwrite
(those three sub-effects were ported on the equip side). But `applyArmor` is called from **only** the equip
path — `setSlot` at inventory.cpp:481 (`+1` on equip) and :452 (`-1` on unequip). It is **never** called from
the consume path in `use()`. The consume branch applies only the nutrition heal (line 1014) and fires
`on_state[0]` (line 1025).

## Divergence

When an NPC consumes an item via `AI_UseItem` → `Inventory::use()` (a combat NPC drinking/eating a scripted
item, or any consumable carrying `protection[]`, a positive `change_atr[]`, or a `disguise_guild`), the original
applies those effects to the consumer through `AddItemEffects`; OpenGothic applies them only when the item is
*equipped*, so a consumed item with those fields grants nothing. This is distinct from the already-fixed
"equipped change_atr applyArmor" (that fix covered the equip path only) and from the food-nutrition heal.

## Proposed patch

Mirror the original's `AddItemEffects` call inside the FOOD-consume branch by reusing the existing
`applyArmor` helper with `sgn=+1` (no paired `-1`, matching the original's permanent application on a consumed
item). Gate it on the same `ITM_CAT_FOOD` condition as the nutrition heal, exactly as the original gates both on
`HasFlag(0x20)`.

OLD (inventory.cpp ~1013–1014):
```cpp
  if(mainflag & ITM_CAT_FOOD)
    owner.changeAttribute(Attribute::ATR_HITPOINTS, itData.nutrition, false);
```
NEW:
```cpp
  if(mainflag & ITM_CAT_FOOD) {
    owner.changeAttribute(Attribute::ATR_HITPOINTS, itData.nutrition, false);
    // NOTE: in original-game oCNpc::UseItem (Gothic2.exe 0x0073bc10) the FOOD/consumable branch
    // (HasFlag 0x20) calls AddItemEffects (0x007320f0, at 0x0073bd0a) after the nutrition heal, applying
    // the consumed item's protection[]/change_atr[]/disguise_guild to the consumer. As the item is then
    // removed (no paired RemoveItemEffects), the bonus is permanent. OpenGothic invoked applyArmor only on
    // the equip path (setSlot), so these fields did nothing when an item was consumed via AI_UseItem.
    applyArmor(*it, owner, 1);
    }
```

Grep-verified symbols: `Inventory::applyArmor(Item&,Npc&,int32_t)` (inventory.h:139), `ItmFlags::ITM_CAT_FOOD`
(constants.h:328), `itData.nutrition`/`change_atr`/`change_value`/`protection`/`disguise_guild` (zenkit IItem,
applied in applyArmor at inventory.cpp:891–922).

**Caveat / why Medium:** `AddItemEffects` also invokes the item's effect *script function* (oCItem field
`0x1e8`, `CallFunc` with `SELF`) and runs `CheckAllCanUse`; the proposed patch covers only the protection/
change_atr/disguise sub-effects (the ones OG already has a faithful helper for). Vanilla Gothic 2 consumables
almost never set protection/change_atr/disguise_guild (permanent potions use `on_state` scripts calling
`Npc_ChangeAttribute`, not the item fields), so visible impact on stock content is small — the value is exact
parity and correct behavior for mods that do use these fields on consumables. The script-function half of
`AddItemEffects` is DEFERRED (its field semantics and the on_state-vs-effect-func ordering need confirmation
before binding a parser call on the consume path).
