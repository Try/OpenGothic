# Inventory info-box value line ignores item condition (shows raw script value)

**Confidence:** Medium-High

## Original function + address

`oCItemContainer::DrawItemInfo` (Gothic2.exe `0x00706e40`) renders the item
info-box: the description line, then a fixed loop over the six text/count rows
(`oCItem::GetText(i)` `0x007120f0` paired with `oCItem::GetCount(i)` `0x00712160`,
which return `TEXT[i]` and `COUNT[i]`). Each row is shown only when its
`COUNT[i] != 0`.

The crucial detail is the **last row (index 5)**. For rows `i < 5` the engine
prints `COUNT[i]` on the right. For `i == 5` it does **not** print `COUNT[5]`;
instead it calls `oCItem::GetValue()` (`0x00712650`) and prints that. `GetValue`
returns the value scaled by item condition: `ceil(value * hp / hp_max)` when
`hp_max > 0`. In trade mode (container status == 5) the engine further multiplies
that by `TRADE_VALUE_MULTIPLIER` (rounded, clamped to a minimum of 1 when
`GetValue() > 0`). So the 6th info row is always the *engine-computed item value*,
not the raw script `COUNT[5]`; `COUNT[5]` only acts as the visibility gate.

## OpenGothic file:line

`/Users/admin/Downloads/opengothic/game/ui/inventorymenu.cpp:736-754` (`InventoryMenu::drawInfo`)

## Divergence

OpenGothic's `drawInfo` prints `r.uiValue(i)` (= `hitem->count[i]`, the raw
script `COUNT[i]`) for every row, and only overrides the last row's number in the
single case "Trade mode AND this is the player's own page", where it substitutes
`r.sellCost()`:

```cpp
int32_t val = r.uiValue(i);          // raw COUNT[i]
...
if(i+1==Item::MAX_UI_ROWS && state==State::Trade && player!=nullptr && pg.is(&player->inventory())){
  val = r.sellCost();
}
```

Consequently, in the **normal (non-trade) inventory / chest / container info-box**,
the last row shows `COUNT[5]` — the un-scaled script `value` — whereas the
original shows `GetValue()` = the condition-scaled value (`cost()` in OG terms).
For a full-condition item (`hp == hp_max`, the vanilla norm) these are equal, so
the bug is invisible; for a **damaged** item the original reduces the displayed
value proportionally while OpenGothic keeps showing the full value. The info-box
value line never passes through OG's already-corrected `cost()` /condition logic.

## Proposed patch

Make the last row's number the engine item value in the general case, keeping the
existing player-side sell-price override for trade:

```cpp
// OLD
    if(i+1==Item::MAX_UI_ROWS && state==State::Trade && player!=nullptr && pg.is(&player->inventory())){
      val = r.sellCost();
      }
```
```cpp
// NEW
    // NOTE: in original-game oCItemContainer::DrawItemInfo @0x00706e40 the last info row
    // (index 5) prints oCItem::GetValue @0x00712650 (condition-scaled value), not COUNT[5];
    // in trade mode it is further scaled by TRADE_VALUE_MULTIPLIER. COUNT[5] only gates the row.
    if(i+1==Item::MAX_UI_ROWS){
      if(state==State::Trade && player!=nullptr && pg.is(&player->inventory()))
        val = r.sellCost();
      else
        val = r.cost();
      }
```

`cost()` (item.h:81) and `sellCost()` (item.h:82) are public and grep-verified;
`cost()` already implements `ceil(value*hp/hp_max)` matching `GetValue`. This
aligns the non-trade value line (and chest/steal/container views) with the
original.

Residual (out of scope, low impact): on the *trader's* page during trade the
original applies the `TRADE_VALUE_MULTIPLIER` buy-side scaling to the value row;
the patch shows the unscaled `cost()` there (still strictly closer to the
original than the previous raw `COUNT[5]`). That buy-side multiplier is a
separate concern and not addressed here.
