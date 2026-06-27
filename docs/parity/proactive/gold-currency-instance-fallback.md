# Gold currency-instance resolution: missing `TRADE_CURRENCY_INSTANCE` fallback to `ItMi_Gold`

**Confidence:** Medium-High

## Original function + address

`oCItemContainer::GetCurrencyInstanceName` (Gothic2.exe `0x00704810`) is the single
source of truth for "what instance is gold" in the whole currency subsystem. It is consumed
by `oCItemContainer::GetCurrencyInstance` (`0x00704a00`), `CreateCurrencyItem`
(`0x00704aa0`), the trade buy/sell paths `oCViewDialogTrade::OnTransferRight` /
`OnTransferLeft` (`0x0068bb40` / `0x0068b840`), the trade list filter
`oCStealContainer::CreateList` (`0x0070ade0`), and the inventory gold counter
`oCNpcInventory::DrawCategory` (`0x0070dbd0`).

The original resolves the currency name in two stages: it reads the string value of the
Daedalus symbol `TRADE_CURRENCY_INSTANCE`, and **if that symbol is absent or its string is
empty it falls back to the hard-coded literal `"ITMI_GOLD"`** (string at `0x008b6d84`,
which maps to the `ItMi_Gold` instance after the parser's case-folding). The function
therefore *always* yields a usable currency instance; gold is never "unresolved."

## OpenGothic file:line

`game/game/gamescript.cpp:352-357` (the `owner.version().game==2` branch of currency init):

```cpp
auto* currency = vm.find_symbol_by_name("TRADE_CURRENCY_INSTANCE");
itMi_Gold      = currency!=nullptr ? vm.find_symbol_by_name(currency->get_string()) : nullptr;
if(itMi_Gold!=nullptr){ // FIXME
  auto item = vm.init_instance<zenkit::IItem>(itMi_Gold);
  goldTxt = item->name;
  }
```

`itMi_Gold` is the backing field for `GameScript::goldId()` (`game/game/gamescript.h:95`,
field at `:476`).

## Divergence

When `TRADE_CURRENCY_INSTANCE` is missing, or when it exists but its string value does not
resolve to a valid instance symbol, OpenGothic leaves `itMi_Gold == nullptr` (the existing
`// FIXME` even flags the block as incomplete). The original instead falls back to
`ItMi_Gold`, so its currency subsystem keeps working.

Consequences in OpenGothic of `goldId()==nullptr`:
- `Item::isGold()` (`game/world/objects/item.cpp:240`) dereferences
  `world.script().goldId()->index()` — null-deref crash.
- `Npc::sellItem` / `Npc::buyItem` (`game/world/objects/npc.cpp:3621,3629,3648`) dereference
  `owner.script().goldId()->index()` — null-deref crash on any trade.
- `Inventory::goldCount()` would never find gold, so the inventory/trade gold counter and all
  affordability checks read 0.

This is exactly the configuration the engine fallback was written to survive (e.g. Gothic 2
classic / total-conversion script sets that never declare `TRADE_CURRENCY_INSTANCE` and rely
on the engine defaulting to `ItMi_Gold`). Distinct from the already-fixed trade-billing count
and stack-merge owner-overwrite issues.

## Proposed patch

Restore the original's unconditional fallback so the currency instance is always resolved.
`vm.find_symbol_by_name` (used throughout this file) is case-insensitive, matching the
parser case-fold the original relies on for `"ITMI_GOLD"` -> `ItMi_Gold`.

OLD (`game/game/gamescript.cpp:352-353`):
```cpp
    auto* currency = vm.find_symbol_by_name("TRADE_CURRENCY_INSTANCE");
    itMi_Gold      = currency!=nullptr ? vm.find_symbol_by_name(currency->get_string()) : nullptr;
```

NEW:
```cpp
    // NOTE: in original-game oCItemContainer::GetCurrencyInstanceName @0x00704810 a missing or
    // empty TRADE_CURRENCY_INSTANCE symbol is coerced to the hard-coded literal "ITMI_GOLD"
    // (string @0x008b6d84), so the currency instance is always resolvable. OpenGothic left
    // itMi_Gold == nullptr in that case, breaking goldCount() and null-deref'ing isGold()/
    // sell/buy on any gold operation.
    auto* currency = vm.find_symbol_by_name("TRADE_CURRENCY_INSTANCE");
    itMi_Gold      = currency!=nullptr ? vm.find_symbol_by_name(currency->get_string()) : nullptr;
    if(itMi_Gold==nullptr)
      itMi_Gold = vm.find_symbol_by_name("ItMi_Gold");
```

Grep-verified OG symbols: `itMi_Gold` (`gamescript.h:476`), `vm.find_symbol_by_name`
(used at `gamescript.cpp:344-372`), `currency->get_string()` (existing line 353). The `if`
only fires when `itMi_Gold` is currently `nullptr`, so vanilla addon behaviour is unchanged
and the change is non-regressive.
