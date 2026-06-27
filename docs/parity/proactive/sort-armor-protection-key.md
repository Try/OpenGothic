# Inventory display-sort: armor ranked by cost instead of total protection

**Confidence:** High

## Original function + address

The inventory display-sort comparator is `oInventory.cpp` comparator at
`Gothic2.exe @0x00705B80` (the predicate `std::sort`/`oCItemContainer::Sort` feeds
each pair into). It first maps each item's `main_flag` (oCItem field `+0x154`) to a
category id and compares the configured category priority (the runtime table at
`DAT_008b6be0`, populated from the `invCatOrder` INI string by the init routine
`@0x00705160`). When two items share a category it switches on that category id to a
category-specific secondary key:

- COMBAT (NF/FF/MUN): sub-category (NF < FF < MUN), then for melee/ranged
  `oCItem::GetFullDamage @0x00712500` descending, then name.
- ARMOR: the `wear` field (oCItem `+0x18c`) ascending, then
  `oCItem::GetFullProtection @0x00712340` (the sum of the eight per-type
  `protection[]` entries at `+0x190`) **descending**, then name.
- MAGIC: ring/amulet/belt sub-rank (`@0x00705fc0`), then name.
- FOOD: `oCItem::GetHealMode @0x00712180` ascending, then heal amount descending, then name.
- POTION / DOCS / RUNE: name only.
- OTHER/NONE: torch sub-rank (`@0x00706070`), then name.
- Universal fallback: display name via `oCItem::GetText @0x00705EB0`, ascending.

Crucially, neither the comparator nor any of its sub-comparators
(`@0x00705fc0`, `@0x00706070`, `@0x00705eb0`) ever calls `oCItem::GetValue` —
**item cost/value is never a sort key in the original.** Armor is ordered by total
protection, descending.

## OpenGothic file:line

`/Users/admin/Downloads/opengothic/game/game/inventory.cpp:1209-1226`
(`Inventory::less`, the predicate behind `Inventory::sortItems`).

## Divergence

OpenGothic's `less()` zeroes the secondary key only for FOOD/POTION/DOCS/RUNE and
otherwise uses `Item::cost()`. ARMOR therefore falls into the `else` branch and is
ranked by `-cost` (most expensive first). The original ranks armor by summed
`protection[]` descending (most protective first) and never consults cost at all.
Result: with several armors equal on category, OpenGothic shows them ordered by
price while vanilla shows them ordered by protection — a visible reordering of the
armor section of the inventory. (The original's leading `wear` key is effectively
constant for the ARMOR category — all body armor uses the same wear slot; rings,
amulets and belts are not `ITM_CAT_ARMOR` — so protection is the operative key.)

This is distinct from the already-fixed `bestItem()`/EquipBestArmor protection key
(`@0x0074f0b0`); that is the equip-selection path, this is the inventory display
sort (`@0x00705B80`).

## Proposed patch

Grep-verified OG symbols: `Item::mainFlag()` (item.h:61), `Item::cost()`
(item.cpp:330), `Item::handle()` with `.protection[]` and `.damage_total`
(already used in `bestItem` at inventory.cpp:1118/1225), `zenkit::DamageType::NUM`
(used at inventory.cpp:1117), `ITM_CAT_ARMOR` (constants.h:327).

OLD (`game/game/inventory.cpp:1209-1220`):
```cpp
  int32_t lV = 0, rV = 0;
  // NOTE: in original-game inventory sort comparator @0x00705B80 the rune branch (category id 3)
  // falls straight to the name-only tie-break @0x00705EB0 -- value/cost is never a rune sort key,
  // so vanilla runes are strictly alphabetical. OpenGothic applied the -cost tie-break to runes
  // (they're not in the zeroing set), reordering the spell-book roughly "expensive first".
  if(il.mainFlag() & (ItmFlags::ITM_CAT_FOOD | ItmFlags::ITM_CAT_POTION | ItmFlags::ITM_CAT_DOCS | ItmFlags::ITM_CAT_RUNE)) {
    lV = 0;
    rV = 0;
    } else {
    lV = il.cost();
    rV = ir.cost();
    }
```

NEW:
```cpp
  int32_t lV = 0, rV = 0;
  // NOTE: in original-game inventory sort comparator @0x00705B80 the ARMOR branch (category id 2)
  // ranks armor by oCItem::GetFullProtection @0x00712340 (the sum of the per-type protection[]
  // array) DESCENDING; cost/value is never an armor sort key (neither the comparator nor its
  // sub-comparators @0x00705fc0/@0x00706070/@0x00705eb0 ever call oCItem::GetValue). OpenGothic
  // ranked armor by -cost, ordering the armor list "most expensive first" instead of "most
  // protective first". (The original's leading wear-slot key is constant for ITM_CAT_ARMOR.)
  if(il.mainFlag() & ItmFlags::ITM_CAT_ARMOR) {
    for(size_t d=0; d<zenkit::DamageType::NUM; ++d) {
      lV += il.handle().protection[d];
      rV += ir.handle().protection[d];
      }
    }
  // NOTE: in original-game inventory sort comparator @0x00705B80 the rune branch (category id 3)
  // falls straight to the name-only tie-break @0x00705EB0 -- value/cost is never a rune sort key,
  // so vanilla runes are strictly alphabetical. OpenGothic applied the -cost tie-break to runes
  // (they're not in the zeroing set), reordering the spell-book roughly "expensive first".
  else if(il.mainFlag() & (ItmFlags::ITM_CAT_FOOD | ItmFlags::ITM_CAT_POTION | ItmFlags::ITM_CAT_DOCS | ItmFlags::ITM_CAT_RUNE)) {
    lV = 0;
    rV = 0;
    } else {
    lV = il.cost();
    rV = ir.cost();
    }
```

The existing tuple negation (`-lV`, `-rV`) already yields descending order, so a
larger summed protection sorts the armor first, matching the original.
