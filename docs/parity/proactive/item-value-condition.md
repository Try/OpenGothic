# Item value ignores condition (HP/HP_max) scaling

**Confidence:** Medium

## Original function

`oCItem::GetValue` (Gothic2.exe `0x00712650`, `oItem.cpp`).

Clean-room description of the decompiled logic:

- It computes a base value as `value + bonus`, where `value` is the script
  `C_ITEM.value` field (oCItem offset `0x160`) and `bonus` is a runtime
  additive field (offset `0x33c`, zeroed in `oCItem::Init` at `0x00711970`,
  effectively always 0 in normal play).
- If the item's `hp_max` (offset `0x150`) is strictly greater than zero, the
  base value is multiplied by the condition ratio `hp / hp_max`
  (`hp` = offset `0x14c`).
- The product is passed through `ceil` and rounded to int.

So in the original: `GetValue = ceil( value * hp / hp_max )` when `hp_max > 0`,
otherwise just `value`. An item at partial condition (`hp < hp_max`) is worth
proportionally less; a full-condition item (`hp == hp_max`) or an item with
`hp_max == 0` is worth exactly `value` (the common vanilla case).

Field mapping confirmed from the Daedalus `C_ITEM` layout copied by
`CreateInstance` in `oCItem::InitByScript` (`0x00711bd0`) and the zeroing in
`oCItem::Init`: id `0x140`, name `0x144`, name_id `0x148`, hp `0x14c`,
hp_max `0x150`, main_flag `0x154`, flags `0x158`, weight `0x15c`, value `0x160`.

## OpenGothic

`game/world/objects/item.cpp:330-332` — `Item::cost()` returns `hitem->value`
flat, with no condition scaling. `hp` / `hp_max` are read only for
serialization (item.cpp:50, :120) and never used in valuation. `sellCost()`
(item.cpp:334) builds on `cost()`, so the sell price inherits the same flat
value.

## Divergence

For any item whose `hp < hp_max` (partial condition), OpenGothic reports the
full `value` for both buy reference price and sell price, whereas the original
scales the value down by the condition ratio and ceils. Vanilla Gothic 2 items
are almost always created at full condition (`hp == hp_max`) or with
`hp_max == 0`, so the practical impact on the unmodified game is minimal; the
divergence becomes gameplay-visible for content/mods that ship items at reduced
condition. The fix below is a strict no-op for the full-condition / `hp_max==0`
cases, so it is safe to apply unconditionally.

## Proposed patch

```cpp
// OLD (game/world/objects/item.cpp)
int32_t Item::cost() const {
  return hitem->value;
  }

// NEW
int32_t Item::cost() const {
  // NOTE: in original-game oCItem::GetValue scales item value by condition:
  // ceil(value * hp / hp_max) when hp_max>0, otherwise plain value.
  // Vanilla items are usually full-condition (no-op), but partial-condition
  // items (mods, worn gear) must be worth proportionally less.
  int32_t value = hitem->value;
  if(hitem->hp_max>0)
    value = int32_t(std::ceil(double(value)*double(hitem->hp)/double(hitem->hp_max)));
  return value;
  }
```

`<cmath>` is already pulled in transitively (`std::ceil` is used in
`sellCost()` directly below), so no new include is required.
