# Crafting DEF_CREATE_ITEM injects a real inventory item instead of a transient interact-item

**Confidence:** Medium

## Original function + address (prose)
`oCNpc::EV_CreateInteractItem` (Gothic2.exe @ `0x00754890`) is the handler for the
animation event `DEF_CREATE_ITEM`, used by smithing/cooking/alchemy sequences to
spawn the visible work-piece in the NPC's hands (e.g. the glowing half-finished
blade during the forging anim, the food being prepared on the cauldron). Its
behaviour is:

- It creates a fresh `oCItem` *vob* from the event's item-instance name via
  `oCWorld::CreateVob` (type `0x81`).
- It registers/locates a `TNpcSlot` for the event's slot name and parents the vob
  to that slot with `oCNpc::PutInSlot`, tagging the slot's ownership-mode field
  (`TNpcSlot+0x18`) with 1 (slot already existed) or 2 (slot newly created).
- It only promotes the new vob to the "current interact item" (`oCNpc+0x968`) via
  `SetInteractItem` when there was **no** prior `DEF_INSERT_ITEM` (the guarded
  `EV_CreateInteractItem: DEF_CREATE_ITEM w/o previously called DEF_INSERT_ITEM`
  warning path); otherwise the previously-inserted item stays the interact item.
- Crucially it **never calls `PutInInv`** — the created work-piece is a transient,
  slot-owned visual. It exists only as long as a slot references it and is reclaimed
  through the `TNpcSlot` ownership-mode logic in `oCNpc::SetInteractItem`
  (@ `0x0074acc0`) when the slot is cleared / replaced, or freed by
  `DEF_DESTROY_ITEM` (`oCNpc::EV_DestroyInteractItem` @ `0x00754b40`).

Contrast with the sibling events that legitimately mint inventory items:
`DEF_EXCHANGE_ITEM` (`oCNpc::EV_ExchangeInteractItem` @ `0x007546f0`) explicitly
calls `PutInInv` for the replacement item; `DEF_CREATE_ITEM` does not.

## OpenGothic file:line
`game/world/objects/npc.cpp:2299-2304` (`Npc::tickTimedEvt`, `ITEM_CREATE` case):

```
case zenkit::MdsEventType::ITEM_CREATE: {
  if(auto it = invent.addItem(i.item,1,world())) {     // <-- adds a REAL inventory item
    invent.putToSlot(*this,it->clsId(),i.slot[0]);
    }
  break;
  }
```

Grep-verified OG symbols: `Inventory::addItem`, `Inventory::putToSlot`,
`Inventory::clearSlot(owner,slot,remove)` all exist (`game/game/inventory.cpp`),
as do `MdsEventType::ITEM_CREATE/ITEM_DESTROY/ITEM_REMOVE`
(`game/world/objects/npc.cpp:2299,2309-2311`).

## Divergence
OpenGothic models hand/slot visuals as inventory-backed `Item`s, so its
`DEF_CREATE_ITEM` handler calls `Inventory::addItem`, materialising the work-piece
as a **real, counted entry in the NPC/player inventory**. The original keeps the
`DEF_CREATE_ITEM` work-piece out of the inventory entirely (slot-owned vob only).

Observable consequence: a `DEF_CREATE_ITEM` work-piece becomes a persistent
inventory item in OG whenever the cleanup of that slot does **not** destroy the
inventory entry. In OG, `ITEM_REMOVE` is mapped to `clearSlot(*this,"",/*remove=*/false)`
(`npc.cpp:2311`), which detaches the model slot but **keeps the inventory entry**;
only `ITEM_DESTROY` passes `remove=true`. The original's `DEF_REMOVE_ITEM`
(`oCNpc::EV_RemoveInteractItem` @ `0x00754880` → `SetInteractItem(0)`) tears down
the transient vob and leaves nothing behind, because it was never in inventory.
Thus any crafting animation that ends a created work-piece with `DEF_REMOVE_ITEM`
(rather than `DEF_DESTROY_ITEM`) leaks a phantom intermediate item (glowing blade,
half-prepared food, etc.) into the player's inventory in OG, where vanilla shows
nothing. The same created vob is also visible in the inventory screen mid-craft in
OG but not in vanilla.

## Proposed patch
**DEFERRED.**

Reason:
1. **Architectural, not surgical.** OG's model slots (`Inventory::putToSlot` /
   `mdlSlots`) are inventory-backed: `putToSlot` itself does `findByClass`-or-`addItem`,
   so merely deleting the explicit `invent.addItem(...)` line does not stop the item
   from entering inventory. A faithful reimplementation needs a transient,
   non-inventory "interact item" view (mirroring the original's slot-owned vob +
   `TNpcSlot` ownership mode), which is a new mechanism, not a one-line change.
2. **Impact is conditional and unverified here.** Whether vanilla content actually
   leaks depends on the exact `DEF_CREATE_ITEM` / `DEF_REMOVE_ITEM` vs
   `DEF_DESTROY_ITEM` pairing inside the shipped `.MDS` smithing/cooking animations,
   which were not inspected in this pass. If every `DEF_CREATE_ITEM` is paired with a
   `DEF_DESTROY_ITEM`, the OG inventory nets out clean and only the transient
   mid-craft inventory visibility differs. A pre-fix check should dump the smithing
   (`HUMANS_SMITH*`) and cooking anim event lists to confirm the create/remove pairing
   before any change is attempted.

`// NOTE: in original-game oCNpc::EV_CreateInteractItem @0x00754890 the DEF_CREATE_ITEM`
`// work-piece is a slot-owned transient vob (no PutInInv); OpenGothic's ITEM_CREATE`
`// adds it to the real inventory, which can leak a phantom intermediate when the slot`
`// is later torn down with DEF_REMOVE_ITEM (remove=false) instead of DEF_DESTROY_ITEM.`
