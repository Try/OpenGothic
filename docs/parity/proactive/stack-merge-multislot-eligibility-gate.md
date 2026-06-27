# Stack-merge ignores `MultiSlot()` eligibility (over-merges non-stackable / equipped items)

**Confidence:** High (for the divergence). Fix: **DEFERRED** (architectural — see reason).

## Original function + address

`oCNpcInventory::Insert` @ `0x0070c730` decides whether a freshly acquired item is
folded into an existing inventory slot or given a slot of its own. Before it ever
looks for a merge target it first calls `oCItem::MultiSlot` @ `0x007125a0` on the
**incoming** item; the per-slot merge loop then runs only when that returns nonzero,
and inside the loop it also requires `MultiSlot` to be nonzero on the **existing**
candidate slot before summing counts (`existing.count += incoming.count`, then the
incoming item is destroyed). If either side is not `MultiSlot`, no merge happens and
the incoming item is inserted into the sorted list as a separate slot.

`oCItem::MultiSlot` @ `0x007125a0` reads the merged item-flag field and returns
*stackable* (1) when any of these hold: the `ITM_MULTI` flag (bit 21) is set, the
item is ammunition (`ITM_CAT_MUN`, bit 3), the item is food (`ITM_CAT_FOOD`, bit 5),
or it is a melee/ranged weapon (`ITM_CAT_NF`/`ITM_CAT_FF`, bits 1/2) that is **not**
currently equipped (the runtime equipped flag, bit 30, is clear) and lacks the bit-26
flag. Otherwise it returns *not stackable* (0). Two consequences matter here:

1. Plain armour, rings, amulets, documents, mission items, keys, etc. that do **not**
   carry `ITM_MULTI` are **not** stackable — duplicates take separate slots.
2. An item that is currently **equipped** (bit 30) is **never** a valid merge target
   (for the weapon branch), i.e. the original keeps an equipped item in its own slot
   and inserts any newly acquired identical copy as a distinct slot.

## OpenGothic file:line

- `game/game/inventory.cpp:234` — `Inventory::addItem(std::unique_ptr<Item>&&)`
- `game/game/inventory.cpp:264` — `Inventory::addItem(size_t itemSymbol, ...)`

Both merge purely on class identity via `findByClass(cls)` and unconditionally do
`it->setCount(it->count()+...)`. There is no `MultiSlot()`/stackability gate and no
"existing slot is equipped" guard. `Item::isMulti()` (`game/world/objects/item.cpp:251`)
exists but only tests `ITM_MULTI` (bit 21); it is narrower than the original
`MultiSlot()` and, crucially, is **not** consulted by `addItem` at all.

## Divergence

OpenGothic over-merges. Any two items sharing a script instance collapse into one
slot with a summed `amount`, regardless of stackability or equip state:

- Non-`ITM_MULTI` items (armour, rings, amulets, docs, mission items) that the
  original keeps in separate slots are shown/stored as a single stacked slot.
- An already **equipped** item absorbs a newly acquired identical copy
  (`amount` becomes 2 with `equipped==1`), where the original would have created a
  second, independent, unequipped slot.

The equipped case has a concrete observable consequence on corpse/container loot:
the loot iterators skip a slot when `isEquipped()` is true
(`Inventory::Iterator::skipHidden`, `game/game/inventory.cpp:73`/`83`). Because the
spare copy is merged into the single equipped `Item`, the whole stack reports
`isEquipped()==true` and the spare becomes **unlootable**. In the original the spare
sits in its own non-equipped slot and is lootable. (A downed NPC carrying both an
equipped weapon and an identical spare is the trigger; uncommon, but real.)

## Proposed patch — DEFERRED

No surgical, build-verifiable fix. OpenGothic's inventory is built on a
**single-slot-per-class** invariant: `findByClass(cls)` is the sole identity key used
across `addItem`, `delItem`, `itemCount`, `equip`/`unequip`, `transfer`, and
`sortItems`, and "equipped" is modelled as a sub-count (`Item::equipped`) of the one
merged `Item` rather than as a separate slot. Introducing the original's `MultiSlot()`
gate would require `addItem` to create a second slot of the same class (for
non-stackable or equipped items), which `findByClass` and every caller above cannot
represent — `findByClass` would return only the first match, silently breaking
equip/del/trade accounting. A correct fix is a multi-slot redesign of `Inventory`,
not a local edit, so per the "surgical-only, else DEFERRED" rule this is deferred.

Grep-verified symbols referenced: `Inventory::addItem`, `Inventory::findByClass`,
`Item::isMulti` (`item.cpp:251`), `Item::isEquipped` (`item.h:43`),
`Inventory::Iterator::skipHidden` (`inventory.cpp:70`), flags `ITM_MULTI`/`ITM_CAT_MUN`/
`ITM_CAT_FOOD`/`ITM_CAT_NF`/`ITM_CAT_FF` (`game/game/constants.h:323-345`).

// NOTE: in original-game oCNpcInventory::Insert @0x0070c730 a stack-merge is gated on
// oCItem::MultiSlot @0x007125a0 being nonzero for BOTH the incoming and the existing
// slot; non-ITM_MULTI items (armour/rings/amulets/docs/mission) and equipped items are
// never merged and take their own slot.
