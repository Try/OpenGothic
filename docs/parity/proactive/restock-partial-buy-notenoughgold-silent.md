# Partial-stack buy suppresses the PLAYER_TRADE_NOT_ENOUGH_GOLD warning

**Confidence:** Medium (the divergence is verified and reproducible; impact is feedback-only — no
gold/item state difference).

## Original function + address
`oCViewDialogTrade::OnTransferRight` (Gothic2.exe `0x0068bb40`) is the buy-side handler. It moves
the requested items from the merchant's steal-container one unit at a time. For every unit, it reads
the unit's `oCItem::GetValue` and asks the player inventory whether it holds that much currency. When
a unit cannot be paid for, it re-inserts that unit back into the steal-container and raises the
`PLAYER_TRADE_NOT_ENOUGH_GOLD` script callback (resolved through `zCParser::GetIndex` /
`zCEventManager`). The key point: the units the player *can* afford are still purchased in the earlier
loop iterations, and the warning is fired anyway as soon as the first unaffordable unit is reached.
So a "buy 10, can only afford 3" request buys 3 *and* warns the player. Stock shortfall is handled on
a different path (`SetTransferCount`) and does **not** raise this warning — the warning is strictly a
gold-shortfall signal. The merchant's gold is unlimited (payout uses `CreateCurrencyItem`; buy-back
destroys the player's currency via `RemoveCurrencyItem`), so the warning is the only player-visible
consequence of a partial affordability clamp.

## OpenGothic file:line
`/Users/admin/Downloads/opengothic/game/world/objects/npc.cpp:3594` (`Npc::buyItem`), specifically the
affordability clamp at lines 3599-3601 and the `count==0` guard at 3602-3605.

## Divergence
OpenGothic clamps `count` to what the player can afford (`count = goldCount/price`) and only emits
`printCannotBuyError` (the same `PLAYER_TRADE_NOT_ENOUGH_GOLD` callback, verified in
`game/game/gamescript.cpp:1006`) when the clamp drives `count` to **zero**. When the player can afford
part of a multi-unit request (the Ten/Hundred/Stack loot modes in
`game/ui/inventorymenu.cpp:511-531` pass `count>1`), OpenGothic silently buys the affordable subset
and shows nothing, whereas the original still raises the warning. The purchased amount is identical in
both engines; only the missing warning differs.

The UI already caps the requested `count` to the merchant's stock
(`inventorymenu.cpp:515-517`, `itemCount = min(itemCount, it.count())`), so the gold clamp here is
always relative to a count that is `<= stock`; the added warning therefore reflects gold shortfall
only, matching the original's separation of the stock path from the gold path.

## Proposed patch
Grep-verified symbols: `Npc::buyItem`, `Inventory::goldCount`, `Inventory::priceOf`,
`GameScript::printCannotBuyError` (all exist).

OLD (`game/world/objects/npc.cpp`, in `Npc::buyItem`):
```cpp
  int32_t price = from.invent.priceOf(id);
  if(price>0 && size_t(price)*count>invent.goldCount()) {
    count = invent.goldCount()/size_t(price);
    }
  if(count==0) {
    owner.script().printCannotBuyError(*this);
    return;
    }
```

NEW:
```cpp
  int32_t price = from.invent.priceOf(id);
  if(price>0 && size_t(price)*count>invent.goldCount()) {
    count = invent.goldCount()/size_t(price);
    // NOTE: in original-game oCViewDialogTrade::OnTransferRight @0x0068bb40 every requested unit the
    // player cannot pay for raises PLAYER_TRADE_NOT_ENOUGH_GOLD, even when the affordable units are
    // still purchased; a partial-stack buy must warn, not just the buy-nothing case.
    if(count!=0)
      owner.script().printCannotBuyError(*this);
    }
  if(count==0) {
    owner.script().printCannotBuyError(*this);
    return;
    }
```

This adds the warning for the partial-affordability case (`count!=0` after the clamp) while leaving
the existing buy-nothing case (`count==0`) untouched — so the callback fires exactly once in either
scenario.
