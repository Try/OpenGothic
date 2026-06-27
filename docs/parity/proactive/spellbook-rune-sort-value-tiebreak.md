# Spell-book rune ordering: spurious value/cost tie-break reorders runes

**Confidence:** High (vanilla data); the original comparator path for runes is unambiguous.

## Original function + address
`oCInventory` item-sort comparator at `0x00705B80` (the `std::sort` predicate over
`oCItem*`, source `oInventory.cpp`). It first maps each item's `main_flag` to an
internal category id (weapons=1, armor=2, **rune=3**, magic/jewelry=4, food=5,
potion=6, docs=7, misc=8) and compares a cross-category priority table. When two
items share a category it dispatches a per-category tie-break:

- weapons -> sub-type, then full damage, then name;
- armor   -> armor field, then full protection, then name;
- magic/jewelry (`0x80000000`) -> `0x00705FC0` (amulet < ring < belt < other), then name;
- food    -> heal-mode, then name;
- **rune (`ITM_CAT_RUNE`, id 3), potion, docs -> fall straight to the name-only
  comparator `0x00705EB0`** (case-by-byte compare of `oCItem::GetText`).

The crucial fact: the original comparator **never uses the item value/cost as a sort
key for any category**. For runes the only tie-break after category is the item
**name** (alphabetical, ascending). Damage and protection are weapon/armor-only keys.

## OpenGothic file:line
`/Users/admin/Downloads/opengothic/game/game/inventory.cpp:1156` (`Inventory::less`),
specifically the cost block at lines 1165-1172 and the comparison tuple at 1177-1178.

## Divergence
`Inventory::less` applies a *universal* `(-damage_total, -cost, displayName)`
tie-break to every category except FOOD/POTION/DOCS:

```
int32_t lV = 0, rV = 0;
if(il.mainFlag() & (ITM_CAT_FOOD | ITM_CAT_POTION | ITM_CAT_DOCS)) { lV=0; rV=0; }
else { lV = il.cost(); rV = ir.cost(); }
return make_tuple(il.mainFlag(), -il.handle().damage_total, -lV, il.displayName())
    <  make_tuple(ir.mainFlag(), -ir.handle().damage_total, -rV, ir.displayName());
```

Runes (`ITM_CAT_RUNE`) are not in the zeroing set, so OpenGothic sorts runes by
**descending value** first and only falls back to name when two runes have equal
value (`Item::cost()` == `oCItem::GetValue`). Runes have non-uniform values, so the
spell-book / rune-inventory order becomes roughly "expensive first" instead of the
original's strict alphabetical-by-name order. The runes' `damage_total` is 0, so the
preceding `-damage_total` key is inert; the spurious `-cost` term is the sole cause
of the reordering. This changes the on-screen rune order and therefore the position a
newly-learned rune lands at relative to its peers.

## Proposed patch
Add `ITM_CAT_RUNE` to the set whose cost is forced to 0, so runes tie-break on name
only, exactly matching the original case-3 path. (Scrolls/jewelry under
`ITM_CAT_MAGIC` are a related, separate case — original sub-orders amulet/ring/belt
then name — and are intentionally left out of this surgical change to avoid
regressing jewelry ordering; scrolls happen to be "other" so name-only would also be
correct for them, but the jewelry sub-order would be lost, so defer that.)

OLD (`inventory.cpp:1166`):
```cpp
  if(il.mainFlag() & (ItmFlags::ITM_CAT_FOOD | ItmFlags::ITM_CAT_POTION | ItmFlags::ITM_CAT_DOCS)) {
```
NEW:
```cpp
  // NOTE: in original-game inventory sort comparator @0x00705B80 the rune branch (category id 3)
  // falls straight to the name-only tie-break @0x00705EB0 -- value/cost is never a rune sort key.
  if(il.mainFlag() & (ItmFlags::ITM_CAT_FOOD | ItmFlags::ITM_CAT_POTION | ItmFlags::ITM_CAT_DOCS | ItmFlags::ITM_CAT_RUNE)) {
```

Grep-verified symbols: `ItmFlags::ITM_CAT_RUNE` (constants.h:332), `Item::mainFlag()`
(item.h:61), `Item::cost()` (item.cpp:330), `Item::displayName()` (item.h:54). One-line
change, build-verifiable.
