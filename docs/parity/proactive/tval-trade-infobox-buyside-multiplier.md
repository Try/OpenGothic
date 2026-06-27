# Trade item-info box omits the value-multiplier on the merchant (buy) side

**Confidence:** High

## Original function + address
`oCItemContainer::DrawItemInfo` (Gothic2.exe `0x00706e40`) renders the focused
item's info popup. The final stat row (row index 5) starts from the full
`oCItem::GetValue` (`0x00712650`, condition-scaled) and then, *only when the
container's display mode equals 5*, recomputes the displayed number as
`round(value * TRADE_VALUE_MULTIPLIER)` and clamps it to a minimum of 1 when the
raw value is > 0 (`oCItemContainer::GetValueMultiplier` `0x007046f0`, default
0.3). For any other mode the raw full value is shown.

The decisive point is which containers are placed in mode 5. Both trade columns
are: `oCViewDialogInventory::StartSelection` (`0x00689270`) and
`oCViewDialogStealContainer::StartSelection` (`0x0068a7c0`) each call the
underlying container's `Init(...,5)` whenever their trade flag (`+0x104`, set to
1 in both constructors `0x00689020` / `0x0068a300`) is 1. In a trade session
`oCViewDialogTrade` (ctor `0x0068adb0`) instantiates exactly these two dialogs
— `+0xfc` = the player's sell inventory, `+0xf8` = the merchant's buy goods
(steal container) — so *both* sides run in mode 5. Consequently the original
info box shows the discounted sell value `round(mult*value)` (min 1) for items on
both the player's and the merchant's sides while trading. Mode 5 is reachable
only through these trade-only dialogs; the standalone inventory / loot UIs use
other modes and show the full value.

## OpenGothic file:line
`game/ui/inventorymenu.cpp:751` (inside `InventoryMenu::drawInfo`).

## Divergence
OpenGothic applies `sellCost()` (= `round(tradeValueMultiplier*value)`, min 1)
only when the active page is the player's own inventory:

```cpp
if(state==State::Trade && player!=nullptr && pg.is(&player->inventory()))
  val = r.sellCost();
else
  val = r.cost();
```

When the merchant's page is the active one (i.e. while browsing items to buy),
`pg.is(&player->inventory())` is false, so the popup shows `cost()` — the full
value. The original shows `round(mult*value)` there too. Net effect: hovering a
merchant's 100-value item, OpenGothic's info box reads 100 while Gothic2.exe
reads 30 (with the default 0.3 multiplier). The player's own sell side already
matches.

## Proposed patch
Apply the trade multiplier to the value row for the whole trade screen, not just
the player's page (the merchant/buy column is also mode 5 in the original).

OLD (`game/ui/inventorymenu.cpp`, ~line 746):
```cpp
    // NOTE: in original-game oCItemContainer::DrawItemInfo @0x00706e40 the last info row (index 5)
    // prints oCItem::GetValue @0x00712650 (condition-scaled value, ceil(value*hp/hp_max)), not the
    // raw COUNT[5] -- COUNT[5] only gates the row's visibility. OpenGothic showed the raw value, so
    // a damaged item's non-trade info-box displayed its full un-scaled value.
    if(i+1==Item::MAX_UI_ROWS){
      if(state==State::Trade && player!=nullptr && pg.is(&player->inventory()))
        val = r.sellCost();
      else
        val = r.cost();
      }
```

NEW:
```cpp
    // NOTE: in original-game oCItemContainer::DrawItemInfo @0x00706e40 the last info row (index 5)
    // prints oCItem::GetValue @0x00712650 (condition-scaled value, ceil(value*hp/hp_max)), not the
    // raw COUNT[5] -- COUNT[5] only gates the row's visibility. OpenGothic showed the raw value, so
    // a damaged item's non-trade info-box displayed its full un-scaled value.
    // NOTE: in original-game oCItemContainer::DrawItemInfo @0x00706e40 the value row applies the
    // TRADE_VALUE_MULTIPLIER for every container in display-mode 5. Both trade columns are mode 5 --
    // the player's sell inventory (oCViewDialogInventory::StartSelection @0x00689270) and the
    // merchant's buy goods (oCViewDialogStealContainer::StartSelection @0x0068a7c0) -- so the info
    // box shows the discounted sell value round(mult*value) (min 1) on BOTH sides while trading.
    // OpenGothic discounted only the player's own page, leaving the merchant's items at full value.
    if(i+1==Item::MAX_UI_ROWS){
      if(state==State::Trade && player!=nullptr)
        val = r.sellCost();
      else
        val = r.cost();
      }
```

Symbols grep-verified to exist: `Item::MAX_UI_ROWS`, `state` / `State::Trade`
(inventorymenu.h:22), `player`, `Item::sellCost()` / `Item::cost()`
(item.cpp:330,339), `GameScript::tradeValueMultiplier()` (gamescript.h:164).
`pg` remains used elsewhere in `drawInfo`, so dropping it from this one condition
is safe.
