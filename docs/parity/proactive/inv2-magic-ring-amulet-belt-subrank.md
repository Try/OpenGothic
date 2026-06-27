# Inventory MAGIC category: amulet/ring/belt sub-rank, not cost

**Confidence:** High

## Original fn + address
The inventory display-sort comparator `oCItemContainer`'s less-functor lives at
`Gothic2.exe @0x00705B80` (`P:\dev\g2addon\release\Gothic\_ulf\oInventory.cpp`). It maps each
item's `main_flag` (field +0x154) to an internal category id, compares the two ids through the
`invCatOrder`-derived rank table at `DAT_008b6be0`, and on a tie dispatches a per-category
*within-category* sub-comparison via a `switch`:

- case 1 COMBAT (NF/FF/MUN): weapon-type sub-rank, then `GetFullDamage` descending.
- case 2 ARMOR: leading wear-slot key (+0x18c), then `GetFullProtection` descending.
- case 4 MAGIC (`main_flag == 0x80000000`, i.e. rings/amulets/belts): calls the magic
  sub-comparator `FUN_00705fc0`.
- case 5 FOOD: `GetHealMode` then heal amount.
- case 8 OTHER/LIGHT: torch sub-comparator `FUN_00706070`.
- default (RUNE/POTION/DOCS): straight to the name tie-break `@0x00705EB0`.

The MAGIC sub-comparator `FUN_00705fc0` ranks each item purely by a wear-slot flag and then
breaks ties on display name (`@0x00705EB0`, alphabetical ascending). **It never reads value/cost.**
Its rank is:

- `HasFlag(0x400000)` ITM_AMULET -> 0
- else `HasFlag(0x800)`   ITM_RING   -> 1
- else `HasFlag(0x1000000)` ITM_BELT -> 2
- else                                 -> 3

So under the MAGIC tab the original lists amulets first, then rings, then belts, then any other
magic trinket, each group alphabetical, with no value ordering.

## OG file:line
`/Users/admin/Downloads/opengothic/game/game/inventory.cpp:1218` (`Inventory::less`), specifically
the category-zeroing `else if` and the universal tie-break tuple at lines 1244-1256.

## Divergence
`Inventory::less` has no MAGIC branch. A ring/amulet/belt has `mainFlag()==ITM_CAT_MAGIC`, which is
neither `ITM_CAT_ARMOR` nor a member of the zeroed set `{FOOD,POTION,DOCS,RUNE}`, so it falls into
the final `else` and takes `lV = il.cost()`. The result tuple
`(mainFlag, -damage_total, -lV, displayName)` then orders magic items by **cost descending**, then
name. The original orders them by **amulet < ring < belt < other**, then name, with cost playing no
role. Consequence: the magic-items page is sorted "most expensive first" instead of grouped by
slot type, so e.g. amulets and rings interleave by price rather than amulets-then-rings, and the
ordering is unstable against item value changes the original never reacts to.

## Proposed patch (DO NOT apply here — proactive report only)

OLD (`inventory.cpp`, in `Inventory::less`):
```cpp
  int32_t lV = 0, rV = 0;
  // NOTE: in original-game inventory sort comparator @0x00705B80 the ARMOR branch (category id 2)
  ...
  if(il.mainFlag() & ItmFlags::ITM_CAT_ARMOR) {
    for(size_t d=0; d<zenkit::DamageType::NUM; ++d) {
      lV += il.handle().protection[d];
      rV += ir.handle().protection[d];
      }
    }
  ...
  else if(il.mainFlag() & (ItmFlags::ITM_CAT_FOOD | ItmFlags::ITM_CAT_POTION | ItmFlags::ITM_CAT_DOCS | ItmFlags::ITM_CAT_RUNE)) {
    lV = 0;
    rV = 0;
    } else {
    lV = il.cost();
    rV = ir.cost();
    }

  // NOTE: in original-game inventory sort comparator @0x00705B80 every category branch falls
  // through to the universal tie-break @0x00705EB0, which orders items equal on all preceding
  // keys alphabetically (ascending) by display name (oCItem::GetText), not by instance index.
  return std::make_tuple(il.mainFlag(), -il.handle().damage_total, -lV, il.displayName())
      <  std::make_tuple(ir.mainFlag(), -ir.handle().damage_total, -rV, ir.displayName());
```

NEW:
```cpp
  int32_t lV = 0, rV = 0;
  int32_t subL = 0, subR = 0;
  // NOTE: in original-game inventory sort comparator @0x00705B80 the ARMOR branch (category id 2)
  ...
  if(il.mainFlag() & ItmFlags::ITM_CAT_ARMOR) {
    for(size_t d=0; d<zenkit::DamageType::NUM; ++d) {
      lV += il.handle().protection[d];
      rV += ir.handle().protection[d];
      }
    }
  ...
  // NOTE: in original-game inventory sort comparator @0x00705B80 the MAGIC branch (category id 4,
  // main_flag ITM_CAT_MAGIC == rings/amulets/belts) dispatches to the magic sub-comparator
  // @0x00705fc0, which ranks ONLY by wear-slot flag -- amulet(ITM_AMULET) < ring(ITM_RING) <
  // belt(ITM_BELT) < other -- then breaks ties on display name @0x00705EB0; it never reads
  // value/cost. OpenGothic let magic items fall into the cost branch, sorting them by price
  // (most expensive first) instead of grouping amulets-then-rings-then-belts.
  else if(il.mainFlag() & ItmFlags::ITM_CAT_MAGIC) {
    lV = 0;
    rV = 0;
    auto rank = [](const Item& it) -> int32_t {
      const uint32_t f = uint32_t(it.itemFlag());
      if(f & ItmFlags::ITM_AMULET) return 0;
      if(f & ItmFlags::ITM_RING)   return 1;
      if(f & ItmFlags::ITM_BELT)   return 2;
      return 3;
      };
    subL = rank(il);
    subR = rank(ir);
    }
  else if(il.mainFlag() & (ItmFlags::ITM_CAT_FOOD | ItmFlags::ITM_CAT_POTION | ItmFlags::ITM_CAT_DOCS | ItmFlags::ITM_CAT_RUNE)) {
    lV = 0;
    rV = 0;
    } else {
    lV = il.cost();
    rV = ir.cost();
    }

  // NOTE: in original-game inventory sort comparator @0x00705B80 every category branch falls
  // through to the universal tie-break @0x00705EB0, which orders items equal on all preceding
  // keys alphabetically (ascending) by display name (oCItem::GetText), not by instance index.
  // subL/subR carry the MAGIC wear-slot sub-rank (0 for every other category, so inert there).
  return std::make_tuple(il.mainFlag(), -il.handle().damage_total, -lV, subL, il.displayName())
      <  std::make_tuple(ir.mainFlag(), -ir.handle().damage_total, -rV, subR, ir.displayName());
```

Notes for the implementer: `Item::itemFlag()` returns the merged runtime flag field (the same one
`oCItem::HasFlag` reads, mainflag OR'd into flags by `oCItem::InitByScript @0x00711bd0`), and is
already used elsewhere in this file (e.g. `setSlot`, `updateShieldView`) for `ITM_RING|ITM_AMULET|
ITM_BELT` tests. The new `subL/subR` key is inserted after `-lV` and before `displayName`, so it
is the within-MAGIC primary key (damage_total and lV are both 0 for magic trinkets) and a no-op for
all other categories.
