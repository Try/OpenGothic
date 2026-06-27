# Inventory cursor moves to the *previous* item after consuming a stack, instead of staying on the slot index

**Confidence:** Medium-High

## Original function + address (prose only)

When an item is taken out of a container in the original game, the removal goes through
`oCNpcInventory::Remove` @ `0x0070cbe0` (and `oCNpcInventory::RemoveByPtr` @ `0x0070cc70` for
the single-instance path). Both unlink the item's list node and then immediately call, through the
container vtable slot at offset `0xa8`, `oCItemContainer::CheckSelectedItem` @ `0x00709660`
(vtable entry `0x0083c554`; the slot map is `0x98`=`NextItem` `0x00709740`, `0xa0`=`PrevItem`
`0x00709b00`, `0xa8`=`CheckSelectedItem`, `0xac`=`TransferItem`).

`CheckSelectedItem` is the single point that re-validates the selection after the contents change.
Its logic is:
- If `GetSelectedItem()` still returns a valid item *and* the container's "skip-flagged-items" mode
  field (offset `0x98`, initialized to 0 in the constructor) is 0, it returns immediately and leaves
  the selection index (`this+0x24`) untouched.
- If the list is now empty it sets the index to `-1` and scroll to 0.
- If the list is non-empty and the index is `< 0` it sets the index to 0.
- It never decrements the index and never clamps a too-high index downward.

`GetSelectedItem` @ `0x007092c0` walks the list and returns the item at the *current* index. The
net effect: after you consume the last unit of a stack that sits in the **middle** of the list, the
index is preserved and the item that shifts up into that slot becomes the highlighted one (the
"next" item). The cursor does **not** step back to the preceding item.

## OpenGothic file:line

`game/ui/inventorymenu.cpp:461-485` — `InventoryMenu::onItemAction`, specifically lines 474-477:

```cpp
player->useItem(clsId,slotHint,false);
auto it2 = page.get(sel.sel);
if((!it2.isValid() || it2->clsId()!=clsId) && sel.sel>0)
  --sel.sel;
```

## Divergence

`useItem` either equips (item stays in the list, same `clsId` at the slot → no decrement) or
consumes (eating food / drinking a potion). When the **last** unit of a consumable stack is used,
the stack is removed and the list compacts. OpenGothic then sees `it2->clsId()!=clsId` and, when
`sel.sel>0`, decrements the selection — moving the highlight to the **previous** item.

The original keeps the index fixed (`CheckSelectedItem` returns early because the slot is still
valid), so the highlight lands on the **next** item that slid up into the slot.

Concrete repro — inventory `[A, B(×1), C]`, cursor on `B` (index 1), use the last `B`:
- Original: index stays 1 → highlights `C`.
- OpenGothic: `it2 = C`, `clsId != B`, `sel.sel>0` → `--sel.sel` → highlights `A`.

(At the very end of the list the two differ less: the original would leave the index out of range
until the next move, while OpenGothic's `adjustScroll` at line 544 already clamps to the last slot;
that end-of-list case is the same with or without this decrement, so the meaningful divergence is
the mid-list case above.)

## Proposed patch

Remove the post-`useItem` step-back so the selection index is preserved across a consume, matching
`CheckSelectedItem`. `adjustScroll` (already called after `onItemAction` in `keyDownEvent`/
`mouseDownEvent`) still clamps the index into range when the consumed item was the last list entry.

OLD (`game/ui/inventorymenu.cpp`, in `onItemAction`):
```cpp
    if(it.isEquipped() && slotHint==Item::NSLOT) {
      player->unequipItem(clsId);
      } else {
      player->useItem(clsId,slotHint,false);
      auto it2 = page.get(sel.sel);
      if((!it2.isValid() || it2->clsId()!=clsId) && sel.sel>0)
        --sel.sel;
      }
```

NEW:
```cpp
    if(it.isEquipped() && slotHint==Item::NSLOT) {
      player->unequipItem(clsId);
      } else {
      // NOTE: in original-game oCNpcInventory::Remove @0x0070cbe0 / RemoveByPtr @0x0070cc70 the
      // removal re-validates the cursor through oCItemContainer::CheckSelectedItem @0x00709660
      // (vtable slot 0xa8), which preserves the selection index whenever it still points at a
      // valid item. After consuming the last of a mid-list stack the index is kept, so the item
      // that shifts up into the slot becomes highlighted -- the cursor is NOT stepped back to the
      // preceding item. adjustScroll() clamps the index if the consumed item was the last entry.
      player->useItem(clsId,slotHint,false);
      }
```

(Grep-verified OpenGothic symbols used/affected: `Npc::useItem`, `Inventory::Iterator::clsId`,
`InventoryMenu::PageLocal::sel`, `InventoryMenu::adjustScroll`, `Item::NSLOT` — all present in
`game/ui/inventorymenu.cpp`.)
