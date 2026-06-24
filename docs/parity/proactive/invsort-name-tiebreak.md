# Inventory sort: final tie-break should be alphabetical by display name, not instance index

**Confidence:** High (for the tie-break divergence specifically; broader per-category
sub-key differences are noted as context but intentionally left out of the surgical patch).

## Original function + address (prose only)

The player/NPC inventory in `Gothic2.exe` keeps its item list in sorted order through a
single shared comparator used by both `oCNpcInventory` and `oCItemContainer`. Both classes
embed a `zCListSort<oCItem>` whose comparator (vtable slot 0) is set in their constructors
to the same routine at **0x00705B80** (referred to here as the "inventory sort comparator";
its in-game source is `oInventory.cpp`). `zCListSort<oCItem>::InsertSort` (0x007110A0) walks
the list and inserts a new item *before* the first existing item for which
`comparator(newItem, listItem) < 0`, so the comparator fully defines display order.

The comparator works in stages:

1. **Primary key — category.** It maps each item's `mainflag` to a small category id via the
   same logic as `oCNpcInventory::GetCategory` (0x0070C690): MAGIC->4, ARMOR->2, NF/FF/MUN->1,
   RUNE->3, FOOD->5, POTION->6, DOCS->7, NONE/other->8. That id then indexes a priority table
   at `0x008B6BE0` to produce the actual ordering rank.

2. **Secondary keys — per category.** Within a category the comparator branches:
   weapons (case 1) by sub-class (NF<FF<MUN) then `GetFullDamage` (0x00712500, the sum of the
   8 damage fields) descending; armor (case 2) by item field `+0x18C` descending then
   `GetFullProtection` (0x00712340, sum of the 8 protection fields) descending; magic/rune
   (case 4) via 0x00705FC0; food (case 5) by `GetHealMode` then heal value; etc.

3. **Universal tie-break.** *Every* branch (and the default branch for runes/potions/docs)
   falls through to **0x00705EB0**, which fetches each item's display name via virtual
   `oCItem::GetText`/visual-name accessor (vtable+0x7C, backed by `oCItem::GetText` 0x007120F0)
   and performs a raw lexicographic byte comparison of the two name strings, returning -1 when
   the new item's name sorts before the list item's name. In other words, items that are equal
   on all preceding keys are ordered **alphabetically by display name (ascending)**.

## OpenGothic file:line

`game/game/inventory.cpp:1131-1132` — `Inventory::less`, the comparator driving
`Inventory::sortItems` (`game/game/inventory.cpp:1104`).

```cpp
return std::make_tuple(il.mainFlag(), -il.handle().damage_total, -lV, -il.clsId())
    <  std::make_tuple(ir.mainFlag(), -ir.handle().damage_total, -rV, -ir.clsId());
```

## Divergence

OpenGothic uses `-clsId()` — the Daedalus instance index (symbol declaration order) — as the
final tie-break key. The original uses the item's **display name in ascending alphabetical
order** as its universal final tie-break (routine 0x00705EB0, reached from every category
branch).

Observable effect: two distinct item instances that land in the same category and tie on the
preceding numeric keys (e.g. several plants, several misc/`ITM_CAT_NONE` items, or same-value
trade goods) are listed in the original in alphabetical name order, but in OpenGothic in
instance-declaration order. The relative order of such items in the inventory grid differs.

Grep-verified OpenGothic symbols used by the patch:
- `Item::displayName()` returns `hitem->name` — `game/world/objects/item.cpp:205`, declared
  `game/world/objects/item.h:54` (`std::string_view displayName() const;`).
- `std::string_view` already participates in `std::make_tuple`'s lexicographic `operator<`,
  giving ascending alphabetical order matching the original's -1-on-lesser-name semantics.

(Scope note: the per-category secondary keys in `less()` also diverge from the original — most
visibly armor is effectively ordered by `cost()` rather than by field `+0x18C` then total
protection, since `damage_total` is 0 for armor. Those are separate, larger changes and are
deliberately NOT included below; only the isolated, build-verifiable tie-break is proposed.)

## Proposed patch

OLD (`game/game/inventory.cpp:1131-1132`):
```cpp
  return std::make_tuple(il.mainFlag(), -il.handle().damage_total, -lV, -il.clsId())
      <  std::make_tuple(ir.mainFlag(), -ir.handle().damage_total, -rV, -ir.clsId());
```

NEW:
```cpp
  // NOTE: in original-game inventory sort comparator @0x00705B80 every category branch falls
  // through to the universal tie-break @0x00705EB0, which orders equal items alphabetically
  // (ascending) by display name (oCItem::GetText / vtable+0x7C), not by instance index.
  return std::make_tuple(il.mainFlag(), -il.handle().damage_total, -lV, il.displayName())
      <  std::make_tuple(ir.mainFlag(), -ir.handle().damage_total, -rV, ir.displayName());
```

The `mainFlag` / `-damage_total` / `-value` keys are kept as-is (these remain OpenGothic's
existing approximation of the category + numeric ordering); only the final tie-break element is
switched from the instance-index `-clsId()` to the original's alphabetical `displayName()`.
