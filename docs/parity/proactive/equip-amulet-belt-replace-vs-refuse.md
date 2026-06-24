# Equip parity: amulet/belt slot replaces current instead of refusing when occupied

**Confidence:** High

## Original function + address

`oCNpc::Equip` (Gothic2.exe @ 0x00739c90) is the single entry point used by both the
inventory UI and the script `EquipItem` event. For category-4 items (rings / amulets /
belts / misc effect items) it enforces a per-category equip limit by *scanning the whole
inventory and counting items that carry the category flag and are already marked equipped*
(equipped marker flag 0x40000000):

- Amulet items (flag 0x400000): the routine unpacks the category and iterates every item;
  if it finds **any** item that has the amulet flag **and** is already equipped, it
  `return`s immediately — i.e. it **refuses** to equip a second amulet. It never unequips
  the worn amulet first.
- Belt items (flag 0x1000000): identical logic — if any belt is already equipped, it
  `return`s and refuses.
- Ring items (flag 0x800): it counts equipped rings; once the count would reach 2 it
  `return`s, refusing a third ring.

Armor (category 2, `oCNpc::EquipArmor` @ ~0x00824700) is the only equip path that *does*
explicitly `UnequipItem` the worn piece before equipping the new one, so armor is a true
swap. Rings/amulets/belts are NOT swapped in the original — the existing piece must be
unequipped manually (by clicking it / NSLOT toggle) before a new one can go on.

The flag values line up exactly with OpenGothic's constants:
`ITM_RING = 1<<11 = 0x800`, `ITM_AMULET = 1<<22 = 0x400000`,
`ITM_BELT = 1<<24 = 0x1000000` (game/game/constants.h:335,346,347).

## OpenGothic file:line

`game/game/inventory.cpp:909-913` (inside `Inventory::use`).

Reachable path: `InventoryMenu::onItemAction` (game/ui/inventorymenu.cpp:474) →
`Npc::useItem` (game/world/objects/npc.cpp:3690) → `Inventory::use`. When the selected
item is *not* currently equipped, the amulet/belt branch is entered.

## Divergence

The ring branch already matches the original (refuse-when-full: it only calls `setSlot`
when `ringL` or `ringR` is `nullptr`, otherwise `return false`). But the amulet and belt
branches unconditionally call `setSlot(amulet,...)` / `setSlot(belt,...)`. Because
`Inventory::setSlot` unequips whatever already occupies the slot before equipping the new
item (game/game/inventory.cpp:429-454), the amulet/belt branches perform a silent **swap**.

Result: with an amulet (or belt) already worn, clicking a different amulet (or belt) in the
inventory instantly removes the old one and equips the new one in a single click. The
original game refuses that action — the player must first unequip the worn amulet/belt.
This also changes side effects ordering: the original fires no on_unequip/on_equip at all
for the rejected item, whereas OpenGothic fires on_unequip for the old piece and on_equip
for the new one. The same asymmetry the OG author already encoded for rings (refuse) should
apply to amulets and belts.

## Proposed patch

Make the amulet and belt branches refuse when their single slot is already occupied,
mirroring the ring branch and the original's count-based refusal. (Re-equipping the *same*
item is already handled upstream by the UI's `isEquipped() && slotHint==NSLOT` unequip
branch, and `Inventory::equip` early-returns when `it->isEquipped()`, so this only blocks
swapping in a *different* amulet/belt — exactly the original behavior.)

OLD (game/game/inventory.cpp:909-913):
```cpp
  if(flag & ITM_BELT)
    return setSlot(belt,it,owner,force);

  if(flag & ITM_AMULET)
    return setSlot(amulet,it,owner,force);
```

NEW:
```cpp
  if(flag & ITM_BELT) {
    // NOTE: in original-game oCNpc::Equip (Gothic2.exe 0x00739c90) the belt branch scans the
    // inventory and refuses (returns) when a belt is already equipped; it never swaps. Only
    // EquipArmor swaps. Match the ring branch: refuse when the single belt slot is occupied.
    if(belt!=nullptr)
      return false;
    return setSlot(belt,it,owner,force);
    }

  if(flag & ITM_AMULET) {
    // NOTE: in original-game oCNpc::Equip (Gothic2.exe 0x00739c90) the amulet branch refuses
    // (returns) when an amulet is already equipped; it does not swap. Refuse when occupied.
    if(amulet!=nullptr)
      return false;
    return setSlot(amulet,it,owner,force);
    }
```

Grep-verified OG symbols used: `ITM_BELT`, `ITM_AMULET` (game/game/constants.h:347,346);
fields `belt`, `amulet` (game/game/inventory.cpp:121-122 / 174-175 / 369-374);
`Inventory::setSlot` signature `(Item*&, Item*, Npc&, bool)` (inventory.cpp:408).
