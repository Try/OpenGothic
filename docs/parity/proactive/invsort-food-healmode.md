# Inventory sort: FOOD tab ordered by heal-mode + amount, not by name

**Confidence:** High (divergence existence). Medium-High (exact reimplementation).

## Original fn + address

The in-game inventory display comparator is `oCInventory`'s item compare
`FUN_00705b80` (Gothic2.exe `0x00705B80`, source `oInventory.cpp`). It first maps each
item's `main_flag` (oCItem+0x154) to a category id (weapons/COMBAT=1, ARMOR=2, RUNE=3,
MAGIC=4, FOOD=5, POTION=6, DOCS=7, OTHER=8) and orders categories by a configurable rank
table `DAT_008b6be0` populated from the `[GAME] invCatOrder` ini list by
`FUN_00705160 (0x00705160)`. The OpenGothic default `invCatOrder`
("COMBAT,POTION,FOOD,ARMOR,MAGIC,RUNE,DOCS,OTHER,NONE") matches the original's category
order, so the **tab order is faithful**.

Within a category the comparator dispatches on the category id via a `switch`:
- case 1 (weapons): NF<FF<MUN sub-rank, then `GetFullDamage` descending, then name.
- case 2 (ARMOR): protection descending, then name. *(already fixed)*
- case 4 (MAGIC): `FUN_00705fc0` wear-slot sub-rank amulet<ring<belt. *(already fixed)*
- **case 5 (FOOD): `oCItem::GetHealMode (0x00712180)`** — primary key = heal **mode**
  ascending, secondary = heal **amount** descending, then name.
- case 8 (OTHER): `FUN_00706070`, name-based (with a burning-torch name normalization).
- cases 3 (RUNE), 6 (POTION), 7 (DOCS): no switch case → fall straight to the
  name-only tie-break `FUN_00705eb0 (0x00705EB0)`.

`oCItem::GetHealMode (0x00712180)`, for an item with the FOOD bit (`flags & 0x20`):
returns mode `0` with amount = `nutrition` if `nutrition > 0`; otherwise scans the
`change_atr[]`/`change_value[]` pairs (3 entries) and returns mode `0` (HP, ATR index 0),
`2` (Mana, ATR index 2) or `4` (Strength, ATR index 4) for the first entry with a positive
value at that attribute; otherwise returns mode `10`. So FOOD items group HP-healing
first, then mana, then strength, then non-healing, and within a mode the larger heal
amount sorts first.

## OG file:line

`/Users/admin/Downloads/opengothic/game/game/inventory.cpp:1261-1267` (the
`Inventory::less` comparator).

## Divergence

OpenGothic lumps `ITM_CAT_FOOD` together with `ITM_CAT_POTION | ITM_CAT_DOCS |
ITM_CAT_RUNE` into the "value is never a key" branch that zeroes `lV/rV`, so FOOD items
fall through to the universal **name-only** tie-break. The original singles FOOD out
(switch case 5) and orders it by heal-mode then heal-amount before name — a distinction
none of POTION/DOCS/RUNE share (those are genuinely name-only in the original too). Result:
the food tab in OG is strictly alphabetical, whereas vanilla Gothic II groups it by what is
healed (HP foods, then mana, then strength potions/items) and, inside a group, strongest
first. This is a fresh divergence, distinct from the three excluded fixes (armor
protection sort, MAGIC sub-rank, equip-best weapon spread, trade info-box).

Verified OG symbols: `Item::handle().nutrition`, `Item::handle().change_atr[]`,
`Item::handle().change_value[]`, `zenkit::IItem::condition_count`,
`Attribute::ATR_HITPOINTS=0 / ATR_MANA=2 / ATR_STRENGTH=4`
(`game/game/constants.h:473-477`) — all already used elsewhere in `inventory.cpp`.

## Proposed patch

The existing tuple `(mainFlag, -damage_total, -lV, subL, displayName)` already compares
`-lV` before `subL`. For FOOD, place the heal **mode** in the `-lV` slot (ascending) and the
heal **amount** in the `subL` slot (descending), reproducing the original's mode-then-amount
order without touching the tuple shape. Remove `ITM_CAT_FOOD` from the name-only branch.

OLD (`inventory.cpp:1257-1267`):
```cpp
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

NEW:
```cpp
  // NOTE: in original-game inventory sort comparator @0x00705B80 the FOOD branch (category id 5)
  // dispatches to oCItem::GetHealMode @0x00712180 and ranks food by heal MODE ascending
  // (0=HP via nutrition or change_atr ATR_HITPOINTS, 2=Mana, 4=Strength, 10=none) then by heal
  // AMOUNT descending, then name. OpenGothic lumped FOOD into the name-only branch with
  // POTION/DOCS/RUNE (which really are name-only in the original), making the food tab strictly
  // alphabetical instead of grouped by what is healed. Place the mode in the -lV slot (compared
  // first, ascending) and the amount in the subL slot (compared next, ascending == amount desc).
  else if(il.mainFlag() & ItmFlags::ITM_CAT_FOOD) {
    auto healMode = [](const Item& it, int32_t& amount) -> int32_t {
      auto& h = it.handle();
      if(h.nutrition>0) { amount = h.nutrition; return 0; }
      for(size_t k=0; k<zenkit::IItem::condition_count; ++k) {
        const int32_t atr = h.change_atr[k];
        const int32_t val = h.change_value[k];
        if(val<=0)
          continue;
        if(atr==int32_t(Attribute::ATR_HITPOINTS)) { amount = val; return 0; }
        if(atr==int32_t(Attribute::ATR_MANA))      { amount = val; return 2; }
        if(atr==int32_t(Attribute::ATR_STRENGTH))  { amount = val; return 4; }
        }
      amount = 0;
      return 10;
      };
    int32_t la = 0, ra = 0;
    lV   = -healMode(il, la); // -lV = mode, compared first, ascending
    rV   = -healMode(ir, ra);
    subL = -la;               // -amount, compared next, ascending == amount descending
    subR = -ra;
    }
  // NOTE: in original-game inventory sort comparator @0x00705B80 the rune branch (category id 3)
  // falls straight to the name-only tie-break @0x00705EB0 -- value/cost is never a rune sort key,
  // so vanilla runes are strictly alphabetical. OpenGothic applied the -cost tie-break to runes
  // (they're not in the zeroing set), reordering the spell-book roughly "expensive first".
  else if(il.mainFlag() & (ItmFlags::ITM_CAT_POTION | ItmFlags::ITM_CAT_DOCS | ItmFlags::ITM_CAT_RUNE)) {
    lV = 0;
    rV = 0;
    } else {
    lV = il.cost();
    rV = ir.cost();
    }
```

Note: the original `GetHealMode` priority per `change_atr` entry checks HP, then Mana, then
Strength; since `change_atr[k]` holds a single attribute index, the `if/continue` chain above
is equivalent. `nutrition` is checked first, matching the original.
