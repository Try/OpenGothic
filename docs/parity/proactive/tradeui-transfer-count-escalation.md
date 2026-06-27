# Trade/loot item-transfer count selector: escalation curve diverges (uncapped `pow(10,n/10)` vs accumulate-by-amount capped at 10000)

**Confidence:** Medium-High

## Original function + address (prose)

The original ZenGin computes the per-tick transfer amount from a held *transfer count* through
`oCItemContainer::TransferCountToAmount` @0x007046b0. That helper is a pure step function of the
running transfer counter:

- counter < 10            -> amount 1
- 10 <= counter < 100     -> amount 10
- 100 <= counter < 1000   -> amount 100
- 1000 <= counter < 10000 -> amount 1000
- counter >= 10000        -> amount 10000  (hard cap)

(The `< 10` branch is the expression `((counter < 10) - 1 & 9) + 1`, i.e. 1 below 10 and 10 below 100.)

Both loot paths drive it identically. In the chest/inventory loot handler
`oCItemContainer::HandleEvent` @0x0070a640 the code reads the counter via `GetTransferCount`
(vtbl +0x74), maps it with the inlined `TransferCountToAmount` step function, and then, when an item
is actually moved: if `amount < itemStackCount` it calls `IncTransferCount(amount)`
(`oCItemContainer::IncTransferCount` @0x00705150, `counter += amount`) and transfers exactly
`amount`; otherwise it calls `SetTransferCount(0)` (reset) and transfers the whole remaining stack.
The trade handler `oCViewDialogTrade::HandleEvent` @0x0068b522 calls the very same
`oCItemContainer::TransferCountToAmount` against `oCViewDialogTrade::GetTransferCount`
(`IncTransferCount` @0x0068b040 also `counter += amount`).

Key consequences of the original design:
1. The transfer counter accumulates **by the amount transferred**, not by 1. Because it starts at 0
   and grows by each tick's amount, the counter value always equals the cumulative number of items
   moved so far in the hold.
2. The per-tick amount is **hard-capped at 10000**; the curve never exceeds it no matter how long the
   key is held.

## OG file:line

`game/ui/inventorymenu.cpp:497-504` (`InventoryMenu::onTakeStuff`, `LootMode::Normal` branch):

```cpp
if(lootMode==LootMode::Normal) {
  ++takeCount;
  itemCount = uint32_t(std::pow(10,takeCount / 10));
  if(it.count() <= itemCount) {
    itemCount = uint32_t(it.count());
    takeCount = 0;
    }
  }
```

## Divergence

OpenGothic increments `takeCount` by **1** every tick and derives the amount as
`pow(10, takeCount/10)` with **no upper bound**. This differs from the original on two axes:

- **Increment semantics:** original `counter += amount` (so the tier boundary 10 -> 100 -> 1000 is
  reached after ~9 ticks of the previous tier, and `counter` tracks total taken); OG `takeCount += 1`
  (so each decade is exactly 10 ticks). For long holds on large stacks OG accelerates noticeably
  faster cumulatively (e.g. after 40 ticks OG's amount is already 10000 and climbing while the
  original is still around the 1000 tier).
- **Cap:** original caps the per-tick amount at 10000; OG keeps multiplying — at `takeCount==50`
  (50 x 200ms = 10s hold) the amount is 100000, then 1e6, etc. The subsequent
  `if(it.count() < itemCount) itemCount = it.count();` clamp hides this for stacks <= 10000, but for a
  stack larger than 10000 (e.g. a big gold pile or a scripted mega-stack) the original drains it
  10000-per-tick over several ticks while OG grabs the entire remainder in a single tick once
  `takeCount` crosses the corresponding decade.

This affects all batch-transfer states that share `onTakeStuff` (Chest, Trade, Ransack, and Equip
drop), matching the original where the same `TransferCountToAmount` mapping governs each.

## Proposed patch

```cpp
// OLD
if(lootMode==LootMode::Normal) {
  ++takeCount;
  itemCount = uint32_t(std::pow(10,takeCount / 10));
  if(it.count() <= itemCount) {
    itemCount = uint32_t(it.count());
    takeCount = 0;
    }
  }

// NEW
if(lootMode==LootMode::Normal) {
  // NOTE: in original-game oCItemContainer::TransferCountToAmount @0x007046b0 maps the running
  // transfer counter to a per-tick amount as a step function capped at 10000 (1/10/100/1000/10000),
  // and oCItemContainer::HandleEvent @0x0070a640 accumulates the counter BY THE AMOUNT
  // (IncTransferCount @0x00705150 / @0x0068b040 do counter+=amount), resetting to 0 only when the
  // stack is emptied. OpenGothic instead stepped takeCount by 1 and used an uncapped
  // pow(10,takeCount/10), accelerating faster and never capping at 10000.
  size_t amount = 1;
  if(takeCount>=10000)     amount = 10000;
  else if(takeCount>=1000) amount = 1000;
  else if(takeCount>=100)  amount = 100;
  else if(takeCount>=10)   amount = 10;
  if(amount < it.count()) {
    itemCount  = amount;
    takeCount += amount;
    } else {
    itemCount = it.count();
    takeCount = 0;
    }
  }
```

(Note: `takeCount` is `size_t` in `inventorymenu.h:95`; the original counter is a `short` reset on
`StartSelection`, but OG already resets it on `mouseUpEvent` (line 377). If exact parity of the
reset-on-keyboard-release is also wanted that is a separate, smaller follow-up — keyUpEvent does not
currently zero `takeCount`.)
