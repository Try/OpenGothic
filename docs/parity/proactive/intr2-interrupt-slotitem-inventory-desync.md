# Hit-interrupt wipes the visual slot-item attachments but not the inventory's slot bookkeeping, resurrecting the item on the next view rebuild

**Confidence:** Medium

## Original function + address (prose)

`oCNpc::Interrupt` (Gothic2.exe `0x00735ab0`) is the engine's single anim/state
hard-interrupt entry. With respect to *items mounted on the NPC*, the only attachment it
touches is the one **interact item** held during an item-use animation: when the interact
slot (`oCNpc+0x968`) is non-null it calls `oCNpc::SetInteractItem(this, NULL)`
(`0x0074acc0`) followed by `oCAniCtrl_Human::SearchStandAni`, i.e. it detaches exactly one
item and lets the engine's normal item-event bookkeeping stay consistent. It does **not**
iterate over and tear down arbitrary in-hand / slot-mounted props; `zCModel::StopAnisLayerRange(2,1000)`
(`0x0057f240`) earlier in the function stops *animation layers*, not vob attachments.
Items created during an animation are created and destroyed in pairs by the model
event-tags (`DEF_CREATE_ITEM` / `DEF_INSERT_ITEM` -> `DEF_REMOVE/DESTROY_ITEM`, processed
by `oCNpc::DoAniEvents`), so the original never leaves a "visual present, bookkeeping
gone" (or vice-versa) split.

## OpenGothic file:line

- `game/graphics/mdlvisual.cpp:795-799` — `MdlVisual::interrupt()`:
  ```cpp
  void MdlVisual::interrupt() {
    skInst->interrupt();
    item.clear();                       // <-- wipes ALL slot attachments
    setStateItem(MeshObjects::Mesh(),"");
    }
  ```
  `item` is `std::vector<MeshAttach> item;` (`game/graphics/mdlvisual.h:159`).
- `game/world/objects/npc.cpp:2232` — the **only** caller, inside `Npc::onDamage`'s
  hit-reaction, guarded by `interactive()==nullptr && ((state&BS_FLAG_INTERRUPTABLE) ||
  state==BS_RUN || state==BS_NONE)`.
- `game/game/inventory.cpp:742-763` (`Inventory::putToSlot`) and
  `game/world/objects/npc.cpp:2390-2399` — slot attachments are populated from the
  `ITEM_CREATE` / `ITEM_INSERT` animation event-tags via
  `invent.putToSlot()` / `invent.putCurrentToSlot()`, which push an entry into
  `Inventory::mdlSlots` (`game/game/inventory.h:174`) **and** call `MdlVisual::setSlotItem`
  (i.e. append to the visual `item` vector).
- `game/game/inventory.cpp:504-507` — `Inventory::updateView` re-attaches every
  `mdlSlots` entry back into the visual `item` vector:
  ```cpp
  for(auto& i:mdlSlots) {
    auto vbody = world.addView(i.item->handle());
    owner.setSlotItem(std::move(vbody),i.slot);
    }
  ```

## Divergence

`MdlVisual::interrupt()` clears the **entire** visual `item` vector, but nothing clears the
matching `Inventory::mdlSlots` list (the proper teardown path is `Inventory::clearSlot`,
`game/game/inventory.cpp:765`, which removes from `mdlSlots` *and* calls `clearSlotItem`).

So if an NPC takes an interruptable hit *between* an `ITEM_CREATE`/`ITEM_INSERT` event and
its paired `ITEM_DESTROY`/`ITEM_REMOVE` event (the window during eating / drinking /
crafting / any prop-spawning gesture), `onDamage -> visual.interrupt()` removes the visual
prop while `Inventory::mdlSlots` keeps the stale entry (and, for `ITEM_CREATE`, the spawned
`Item` stays in the inventory). The split then resolves the *wrong* way on the next
`Inventory::updateView` — which runs on save/load (`#907`) and on any equip change — because
that routine walks `mdlSlots` and re-mounts the orphaned attachment: the long-since-gone
prop reappears in the NPC's hand. The original interrupts exactly one interact item via
`SetInteractItem`, never leaving `mdlSlots`/visual desynced, so no such resurrection occurs.

A secondary, smaller mismatch: the original's interrupt detaches only the single interact
item; OpenGothic additionally drops every `Npc_SetSlotItem`-mounted prop on the same hit
(again without notifying inventory), which the original leaves attached.

## Proposed patch

**DEFERRED.**

Reasons:

1. **Not a clean one-liner / wrong layer.** The fix is not to keep `item.clear()` (that
   leaves the prop floating after interrupt) nor to simply delete it; the correct behavior
   is to tear the slot attachments down *through* `Inventory::clearSlot` so `mdlSlots` and
   the visual stay in lock-step, mirroring the `ITEM_DESTROY` event handler
   (`game/world/objects/npc.cpp:2400-2404`). `MdlVisual` has no `Inventory`/`Npc` handle,
   so the cleanup must move up to the caller (`Npc::onDamage` at
   `game/world/objects/npc.cpp:2232`, e.g. `invent.clearSlot(*this,"",true)` alongside
   `visual.interrupt()`), and `MdlVisual::interrupt()` would have to stop owning the raw
   `item.clear()`. That is a multi-site change touching the inventory/visual contract.

2. **Trigger window + manifestation need in-game confirmation.** The desync only bites when
   the hit lands inside the create/destroy event window *and* an `updateView` later fires
   (load or equip change). Whether OpenGothic's event-tag timing reproduces the original's
   create/destroy pairing closely enough for this to be commonly observable (vs. an edge
   case) should be verified before reshaping the interrupt/inventory contract, since several
   item-event animations (cooking `ITEM_EXCHANGE` fallback at npc.cpp:2410, `ITEM_PLACE`)
   route through the same `mdlSlots` and could regress.

<!-- NOTE: in original-game oCNpc::Interrupt @0x00735ab0 only the single interact item is
     detached (oCNpc::SetInteractItem @0x0074acc0 when oCNpc+0x968 != 0); StopAnisLayerRange
     @0x0057f240 stops anim layers, not vob attachments, and create/destroy item event-tags
     keep inventory and visual consistent. OpenGothic MdlVisual::interrupt() clears the whole
     visual `item` vector without updating Inventory::mdlSlots, so updateView re-mounts the
     orphan. -->
