# Merchant trade window hides the merchant's equipped items

**Confidence:** Medium-High

## Original fn + address (prose)

In `Gothic2.exe` the merchant (right-hand) side of the trade window is bound
directly to the merchant's own `oCNpcInventory`. `oCViewDialogTrade::SetNpcRight`
@0x0068b290 just calls `oCViewDialogInventory::SetInventory` with the merchant
NPC's inventory — unlike the corpse/ransack path (`oCNpcContainer::CreateList`
@0x0070b570) it does **not** build a filtered copy of the item list, so the
displayed contents are the merchant's full inventory list.

When the container is opened, `oCNpcInventory::Open` @0x0070bf10 only moves the
NPC's *hand-slot* items (a burning/normal torch, `ITLSTORCHBURNING`/`ITLSTORCH`,
flag 0x20000000) back into the inventory; it performs no filtering of equipped
weapons/armour. The actual per-slot renderer `oCItemContainer::Draw` @0x007076b0
walks every contents item and, when the item carries the equipped flag
`HasFlag(0x40000000)`, merely selects a *different slot-background texture*
(`this+0x6c` / `this+0x74`, the "equipped" highlight) instead of the normal
background (`this+0x68` / `this+0x70`). It never skips equipped items. This is
the same render path used for the player's own inventory, where equipped items
are unambiguously shown (highlighted), confirming equipped items are displayed,
not hidden. Currency is also not skipped in this draw path (the only currency
helper wired into trade, `RemoveCurrencyItem` @0x00704b50, is called from
`OnTransferRight` during a sale, not as a display filter).

Net: the original merchant trade list shows the merchant's equipped items
(weapon/armour the vendor is wearing), drawn with the equipped highlight.

## OG file:line

`game/game/inventory.cpp:70-75` (`Inventory::Iterator::skipHidden`, the
`T_Trade` branch). `T_Trade` is used by exactly one consumer — `TradePage`
(`game/ui/inventorymenu.cpp:60-61`), i.e. the merchant column of the trade
window — so changing it affects only that view.

```cpp
void Inventory::Iterator::skipHidden() {
  auto& it = owner->items;
  if(type==T_Trade) {
    while(at<it.size() && (it[at]->isEquipped() || it[at]->isGold()))
      ++at;
    }
```

## Divergence

OpenGothic's `T_Trade` iterator skips every `isEquipped()` item, so a vendor's
worn armour / wielded weapon disappears from the trade window. The original
keeps equipped items in the merchant's displayed contents (highlighted, per
`oCItemContainer::Draw` @0x007076b0). The `isEquipped()` skip is uncited and has
no counterpart in the original's merchant display path. (The `isGold()` skip is
left untouched — it is conservatively retained and not part of this finding.)

## Proposed patch

```cpp
  if(type==T_Trade) {
    // NOTE: in original-game the merchant trade column is the vendor's own
    // oCNpcInventory rendered directly (oCViewDialogTrade::SetNpcRight @0x0068b290
    // -> SetInventory; no filtered CreateList). oCItemContainer::Draw @0x007076b0
    // draws every contents item and, for an equipped item (HasFlag(0x40000000)),
    // only swaps to the equipped-slot background (this+0x6c/+0x74) -- it never
    // skips it; oCNpcInventory::Open @0x0070bf10 filters nothing but hand-slot
    // torches. OpenGothic additionally hid equipped items, so a merchant's worn
    // armour/weapon vanished from the trade window.
    while(at<it.size() && it[at]->isGold())
      ++at;
    }
```

(OLD: `while(at<it.size() && (it[at]->isEquipped() || it[at]->isGold()))` →
NEW: `while(at<it.size() && it[at]->isGold())`.)
