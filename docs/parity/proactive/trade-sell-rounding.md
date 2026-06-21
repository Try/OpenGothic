# Trade sell price uses ceil instead of round (overpays player)

**Confidence:** High

## Original function + address
`oCViewDialogTrade::OnTransferLeft` (Gothic2.exe `0x0068b840`) handles the
player selling one unit of the selected item to a trader. For each unit it
reads the item's condition-scaled value via `oCItem::GetValue`
(`0x00712650`). If that value is below 1 the item moves with **no** gold
created. Otherwise the gold paid out per unit is computed as
`ROUND(tradeContainer.valueMultiplier * value)` — a round-to-nearest
(half-away-from-zero) of the product — and then clamped to a **minimum of 1**
(`if(payout < 1) payout = 1`). The multiplier is the container's copy of
`TRADE_VALUE_MULTIPLIER` (0.3 in vanilla G2).

So per-unit sell payout = `max(1, round(mult*value))` for value>=1, else 0.

## OpenGothic location
`game/world/objects/item.cpp:339-341` — `Item::sellCost()`:
`int32_t(std::ceil(tradeValueMultiplier()*float(cost())))`.
Used by `Inventory::sellPriceOf` -> `Npc::sellItem` (`npc.cpp:3477`).

## Divergence
OpenGothic uses `std::ceil` where the original uses round-to-nearest. With
mult=0.3 this overpays the player on most sales whose product is not already
near its ceiling, e.g. value=11 -> orig round(3.3)=3, OG ceil=4; value=14 ->
orig 4, OG 5; value=41 -> orig 12, OG 13. The player gets more gold than
vanilla on a large fraction of all sales. (For value=0 items both yield 0; the
original min-1 clamp only bites where round would drop a >=1-value item to 0,
e.g. value=1 -> round(0.3)=0 -> clamped to 1, which ceil also produces.)

## Proposed patch
File: `game/world/objects/item.cpp`

OLD:
```cpp
int32_t Item::sellCost() const {
  return int32_t(std::ceil(world.script().tradeValueMultiplier()*float(cost())));
  }
```

NEW:
```cpp
int32_t Item::sellCost() const {
  // NOTE: in original-game oCViewDialogTrade::OnTransferLeft (Gothic2.exe 0x0068b840)
  // the per-unit sell payout is ROUND(tradeValueMultiplier*value) (round-to-nearest),
  // clamped to a minimum of 1 when the item value is >=1; value 0 pays nothing.
  // OpenGothic previously used std::ceil, which overpaid the player on most sales.
  const int32_t v = cost();
  if(v<=0)
    return 0;
  int32_t price = int32_t(std::lround(world.script().tradeValueMultiplier()*float(v)));
  if(price<1)
    price = 1;
  return price;
  }
```
