# Trade value-multiplier default/clamp divergence (G2: missing/<=0 symbol must fall back to 0.3, not 1.0)

**Confidence:** High (root cause identified, surgical, build-verifiable; in-vanilla impact low because the symbol exists, but the divergence is concrete and reachable for mods / malformed scripts).

## Original function + address

`oCItemContainer::GetValueMultiplier` (Gothic2.exe `0x007046f0`). This static accessor is the
sole source of the per-merchant value multiplier: the trade dialog constructor
`oCViewDialogTrade::oCViewDialogTrade` (`0x0068adb0`) calls it and caches the result into the
container object field at `+0x10c`, which `oCViewDialogTrade::OnTransferLeft` (`0x0068b840`) then
uses as the sell-payout factor `round(field0x10c * GetValue)`, clamped to a minimum of 1.

The accessor works against a static cache (initialized to 0). Its logic, in prose:

- If the cache is exactly `0.0`, it looks up the parser symbol `TRADE_VALUE_MULTIPLIER`
  (via `zCParser::GetSymbol`). If the symbol is found, it reads the symbol's float value into
  the cache.
- It then tests the cache for `<= 0.0`. **If the cache is still `<= 0.0` (symbol missing, OR the
  symbol's value is zero or negative), it forces the cache to `0.3` and returns `0.3`** (the
  constant `0x3e99999a` == 0.30000001f).
- Otherwise it returns the (positive) cached value.

So the original's hard default for a missing/zero/negative `TRADE_VALUE_MULTIPLIER` is **0.3**, and
any non-positive script value is coerced up to **0.3**. The buy side
(`oCViewDialogTrade::OnTransferRight` `0x0068bb10`) uses `oCItem::GetValue` with no multiplier
(buy = full value), confirming the multiplier only governs the sell price.

## OpenGothic file:line

`game/game/gamescript.cpp:358`

```cpp
auto* tradeMul = vm.find_symbol_by_name("TRADE_VALUE_MULTIPLIER");
tradeValMult   = tradeMul != nullptr ? tradeMul->get_float() : 1.0f;
```

(`tradeValMult` declared at `game/game/gamescript.h:477`, consumed via
`tradeValueMultiplier()` `gamescript.h:163` -> `Item::sellCost()` `game/world/objects/item.cpp:347`.)

## Divergence

Two related mismatches versus `oCItemContainer::GetValueMultiplier`:

1. **Missing-symbol fallback.** When `TRADE_VALUE_MULTIPLIER` is absent, OG uses `1.0f`; the
   original uses `0.3f`. With OG=1.0 the player would be paid the full item value on every sale
   (no merchant margin) instead of 30%.
2. **No non-positive clamp.** When the symbol exists but holds `0` or a negative value, OG passes
   that value straight through (`get_float()` -> 0 or negative), so sells pay the minimum 1 gold
   (or, with the existing `if(price<1) price=1` floor in `sellCost()`, also 1). The original
   instead coerces any `<= 0` value to `0.3`, yielding normal 30% payouts.

In stock Gothic II the symbol is defined as `0.3`, so vanilla is unaffected; the divergence is
reachable in mods/total-conversions that omit the symbol or set it to a non-positive value, and it
is a genuine, exactly-characterized behavioral difference.

## Proposed patch

`game/game/gamescript.cpp` (G2 branch only — the Gothic1 branch at line 374 keeps `1.f`, which is
correct for G1 since G1 has no `oCItemContainer::GetValueMultiplier` 0.3 mechanism):

OLD:
```cpp
    auto* tradeMul = vm.find_symbol_by_name("TRADE_VALUE_MULTIPLIER");
    tradeValMult   = tradeMul != nullptr ? tradeMul->get_float() : 1.0f;
```

NEW:
```cpp
    // NOTE: in original-game oCItemContainer::GetValueMultiplier @0x007046f0 a missing
    // TRADE_VALUE_MULTIPLIER symbol -- and any value <= 0 -- is coerced to the hard default 0.3
    // (0x3e99999a); only a strictly-positive script value is used as-is.
    auto* tradeMul = vm.find_symbol_by_name("TRADE_VALUE_MULTIPLIER");
    tradeValMult   = tradeMul != nullptr ? tradeMul->get_float() : 0.3f;
    if(tradeValMult <= 0.f)
      tradeValMult = 0.3f;
```

Grep-verified symbols: `tradeValMult` (gamescript.h:477), `vm.find_symbol_by_name`/`get_float`
(zenkit, already used on the same line). No new fields required.
