# useWithItem: required item is not pulled out of inventory into the held "interact item"

**Confidence:** Medium (root cause decompile-verified; net player-visible effect is
scheme-dependent, so the fix is **DEFERRED** rather than applied).

## Original function + address (prose only)

`oCMobInter::CanInteractWith` (Gothic2.exe @0x00720f40) is the gate that runs when an NPC
commits to using a mob. After it picks a free slot it consults the mob's required item via the
virtual `oCMobInter::GetUseWithItem` (@0x0071db40), which returns the parser symbol index of the
`useWithItem` instance, or a value `< 1` when the mob has none (in which case the whole block is
skipped).

When a required item exists, the original distinguishes whether the NPC already holds it:
the item handling is only performed when the NPC does **not** already have that instance in hand
(`oCNpc::HasInHand`) and it is not already the current interact item. Only then, for the player,
it removes one unit of the item from the inventory (`oCNpc::RemoveFromInv(item,1)`) and makes the
returned standalone `oCItem` object the **interact item** via `oCNpc::SetInteractItem`
(@0x0074acc0). For a non-player NPC that lacks the item, the original instead *creates* the
object (factory) and sets it as the interact item — that NPC-vs-player gate path is the
already-fixed area and is **not** the subject of this note.

The held interact item is then drawn into the mob's named slot on the `DEF_INSERT_ITEM`
animation event by `oCNpc::EV_InsertInteractItem` (@0x007544e0), which requires the interact
item to already be set (it errors `"No slot found"` otherwise), and is released/destroyed on
`DEF_REMOVE_ITEM` / `DEF_DESTROY_ITEM` by `oCNpc::EV_RemoveInteractItem` (@0x00754880) and
`oCNpc::EV_DestroyInteractItem` (@0x00754b40), which call `SetInteractItem(0)` and do **not**
re-add to the inventory. The key point: in the original the required item is moved *out* of the
inventory list to become the held visual; it is a real owned `oCItem`, not a transient class id.

## OpenGothic file:line

`game/world/objects/interactive.cpp:853-856` (`Interactive::attach`):

```cpp
if(!useWithItem.empty()) {
  size_t it = world.script().findSymbolIndex(useWithItem);
  npc.setCurrentItem(it);
  }
```

`Npc::setCurrentItem` → `Inventory::setCurrentItem` (`game/game/inventory.cpp:860-862`) only stores
a class id: `curItem = int32_t(cls);`. On the `ITEM_INSERT` event,
`Inventory::putCurrentToSlot` → `putToSlot` (`game/game/inventory.cpp:723-754`) does
`findByClass(cls)` and attaches a *view* of that inventory item to the model slot — the inventory
entry and its count are never touched (and `delItem` is never called on this path).

## Divergence

OpenGothic treats the `useWithItem` purely as a visual class id: the required item is never
removed from the user's inventory while the mob is in use. The original removes one unit of the
item from the inventory at the moment the interaction is committed (the item physically leaves the
inventory and becomes the held interact-item object), and the `HasInHand` / "already-the-interact-
item" guard means it does this exactly once and skips it when the item is already in hand.

Consequences that differ from vanilla:
- While the mob is being used, the player's inventory count for the required item is one lower in
  the original; in OpenGothic it is unchanged (observable by opening the inventory mid-use).
- Because OpenGothic only attaches a *view* (no ownership transfer), if the required item is the
  player's currently in-hand/equipped item the original takes a no-op path, whereas OpenGothic can
  attach a second visual copy of it to the mob slot.

## Proposed patch — DEFERRED

Reason: the *net* effect of the original's `RemoveFromInv` depends on each mob scheme's animation
events. The remove/destroy interact-item handlers do not re-add to inventory, yet repeatable
item-requiring mobs (e.g. instrument seats) clearly do not consume the item in vanilla — in those
cases the original is reached on the `HasInHand != 0` path (item already drawn) and the
`RemoveFromInv` branch is skipped entirely. Replicating this faithfully requires modelling the
interact-item ownership transfer (remove-on-attach / restore-or-destroy on the matching
`DEF_REMOVE`/`DEF_DESTROY`/`DEF_PLACE` event, plus the `HasInHand`/already-held guard), which is a
structural change, not a surgical one. Applying only the "decrement on attach" half would risk a
false-positive item loss. Verified OpenGothic symbols available for a future faithful
implementation: `Inventory::setCurrentItem`/`curItem`, `Inventory::putToSlot`/`putCurrentToSlot`,
`Inventory::delItem`, `Inventory::itemCount`, `Npc::hasAmunition`/in-hand state, and the
`ITEM_INSERT`/`ITEM_REMOVE`/`ITEM_DESTROY` event handlers in `game/world/objects/npc.cpp:2312-2344`.

// NOTE: in original-game oCMobInter::CanInteractWith @0x00720f40 the useWithItem is taken out of
// the inventory via oCNpc::RemoveFromInv and held as the interact item (oCNpc::SetInteractItem
// @0x0074acc0), guarded by oCNpc::HasInHand; OpenGothic's Interactive::attach only records a
// curItem class id and never removes the item from the inventory.
