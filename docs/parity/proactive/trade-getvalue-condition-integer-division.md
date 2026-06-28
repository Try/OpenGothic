# Trade pricing: condition-scaled item value uses integer division, not float

**Confidence:** Medium (code-level divergence is unambiguous; real-world trigger frequency
in vanilla content — items shipped with `0 < hp < hp_max` — is unverified).

## Original fn + address

`oCItem::GetValue` (Gothic2.exe `0x00712650`) is the single source of truth for an item's
trade value (the sell payout, the buy price, and the info-box value row all route through it).
Decompiled, it computes a value in pure **integer** arithmetic:

- It starts from `base = runtimeBonus + value`, where `runtimeBonus` is a runtime field
  (`oCItem+0x33c`) that `oCItem::Init` @`0x00711970` initialises to `0`, and `value` is the
  script value field (`oCItem+0x160`).
- If `hp_max > 0` (field `oCItem+0x150`), it multiplies `base` by the **integer quotient**
  `hp / hp_max` (`oCItem+0x14c` / `oCItem+0x150`). Both operands are `int`, so this is an
  integer `idiv`: the quotient is `0` whenever `0 < hp < hp_max`, and `1` when `hp == hp_max`.
- A trailing `ceil()`/round is applied, but it is a no-op because every input is integral.

Net effect for a damaged item (`hp_max > 0`, `hp < hp_max`): the multiplier is `0`, so
`GetValue` returns `0`. On the sell path (`oCViewDialogTrade::OnTransferLeft` @`0x0068b840`)
a `GetValue < 1` item takes the "pay nothing" branch — no currency item is created — so the
merchant pays **0 gold** for a damaged item.

Field identification: the offsets `0x14c/0x150/.../0x160` are the consecutive C_ITEM members
`hp, hp_max, mainflag, flags, weight, value` (weight `0x15c`/value `0x160` are confirmed
adjacent by `oCItem::GetWeight` @`0x007126b0`).

## OG file:line

`/Users/admin/Downloads/opengothic/game/world/objects/item.cpp:330-337` (`Item::cost()`).

OpenGothic computes `ceil(value * hp / hp_max)` in **floating point**:

```cpp
if(hitem->hp_max>0 && hitem->hp<hitem->hp_max)
  return int32_t(std::ceil(float(hitem->value)*float(hitem->hp)/float(hitem->hp_max)));
return hitem->value;
```

## Divergence

OpenGothic uses **float** division, yielding a *proportional* value for a damaged item
(e.g. `value=100, hp=50, hp_max=100` → `50`). The original uses **integer** division of
`hp / hp_max`, which is `0` for any `0 < hp < hp_max`, so the same item is worth `0`.
Consequences: a damaged item sells to a merchant for `min 1` gold in OpenGothic
(via `sellCost()` floor) but for `0` gold in the original, and the info-box / buy price differ
identically. (Secondary nuance: the original applies the multiplier whenever `hp_max > 0`,
including `hp > hp_max`, where the integer quotient is `>= 2` and the value is *multiplied*;
OpenGothic's extra `hp < hp_max` guard skips that case. Both agree on the common
`hp == hp_max` / `hp_max == 0` items, which is why this is invisible for undamaged goods.)

## Proposed patch

```cpp
// OLD
int32_t Item::cost() const {
  // NOTE: in original-game oCItem::GetValue (Gothic2.exe 0x00712650) scales the value by
  // condition: ceil(value * hp / hp_max) when hp_max>0. Full-condition items (the vanilla
  // norm, hp==hp_max) are unchanged; a damaged item is worth proportionally less.
  if(hitem->hp_max>0 && hitem->hp<hitem->hp_max)
    return int32_t(std::ceil(float(hitem->value)*float(hitem->hp)/float(hitem->hp_max)));
  return hitem->value;
  }

// NEW
int32_t Item::cost() const {
  // NOTE: in original-game oCItem::GetValue (Gothic2.exe 0x00712650) condition scaling is
  // integer arithmetic: value * (hp / hp_max) with an INTEGER quotient when hp_max>0, then a
  // (no-op) ceil. For 0 < hp < hp_max the quotient is 0, so a damaged item is worth 0 (it
  // sells for nothing); only hp==hp_max leaves the value unchanged. OpenGothic previously used
  // float division, paying a proportional non-zero amount for damaged items.
  if(hitem->hp_max>0)
    return hitem->value * (hitem->hp / hitem->hp_max);
  return hitem->value;
  }
```

This keeps `<cmath>`'s `std::ceil` unnecessary for this path; if no other use remains, the
include can stay (harmless). The patch is surgical and build-verifiable; all referenced OG
fields (`hitem->hp`, `hitem->hp_max`, `hitem->value`, all `int`) are confirmed present and
used by the existing code.
