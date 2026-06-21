# Issue #907 — Stone on pedestal disappears after game load

**Disposition:** FIX (surgical, save/persist parity)

## Summary
A stone placed on a pedestal (Gothic II "DEF_PLACE_ITEM" mechanic) renders correctly
when first placed, but becomes invisible after save+reload and never reappears.

## OG files
- `game/game/inventory.cpp` — `Inventory::moveItem` (places stone), `Inventory::implLoad`,
  `Inventory::updateView`, `Inventory::load(Serialize&, Interactive&, World&)`
- `game/game/inventory.h` — `updateView` declarations, `MdlSlot`
- `game/world/objects/interactive.cpp` — `Interactive::load` (calls `invent.load`)
- `game/graphics/mdlvisual.cpp` — `MdlVisual::setSlotItem` (attaches the slot visual)

## Original-game behavior (prose)
In Gothic2.exe the pedestal is an `oCMobInter` whose placed item is part of the mob's
visual/attachment state. On save the whole world VOB tree (including the mobsi and its
attached slot visual) is archived by `zCArchiverBinSafe`; on load the engine restores the
mobsi visual together with its attachment, so the stone is drawn again automatically.
Relevant restore path is the generic VOB unarchive plus `oCMobInter::SetVisual`
(Gothic2.exe 0x71d3c0), which re-establishes the visual after load.
// NOTE: in original-game the slot/attachment visual is part of the persisted VOB visual,
// so no explicit re-render step is needed — it is implied by visual restore.

## OG current behavior / divergence
OpenGothic does not persist the mobsi visual mesh; instead the placed item is tracked in
the mobsi's `Inventory::mdlSlots` and rendered via `Interactive::setSlotItem`
(`inventory.cpp:796`, set in `moveItem`).

- `Inventory::save` writes `mdlSlots` (`inventory.cpp:160-164`).
- `Inventory::implLoad` reads `mdlSlots` back (`inventory.cpp:101-113`) — data survives.
- BUT the slot **visual** is only re-applied inside `Inventory::updateView(Npc&)`
  (`inventory.cpp:461-473`), which is called from `implLoad` **only when `owner!=nullptr`**
  (`inventory.cpp:142-143`).
- For an Interactive the inventory is loaded via
  `Inventory::load(Serialize&, Interactive&, World&)` (`inventory.cpp:150-152`) which calls
  `implLoad` with `owner==nullptr`. So `updateView` never runs and
  `Interactive::setSlotItem` is never re-invoked.

Result: after load the stone's `MdlSlot` data exists but is never attached to the pedestal
mesh, so it is invisible. `moveItem` only ever fires on the original placement (the slot is
already populated), so it never re-renders → stone never reappears. Matches #907 exactly.

## Proposed patch

### 1) `game/game/inventory.h`
Add an Interactive overload next to the Npc `updateView`.

OLD:
```cpp
    void   updateView      (Npc& owner);
```
NEW:
```cpp
    void   updateView      (Npc& owner);
    void   updateView      (Interactive& owner, World& world);
```

### 2) `game/game/inventory.cpp`
Re-apply the mobsi slot visuals on load. Add the overload after `updateView(Npc&)`
(after `inventory.cpp:482`):

NEW:
```cpp
void Inventory::updateView(Interactive& owner, World& world) {
  // NOTE: in original-game the mobsi slot item is part of the persisted VOB visual and is
  // restored automatically; OpenGothic stores it in mdlSlots, so re-attach it explicitly.
  for(auto& i:mdlSlots) {
    auto vbody = world.addView(i.item->handle());
    owner.setSlotItem(std::move(vbody),i.slot);
    }
  }
```

And call it from the Interactive load overload.

OLD (`inventory.cpp:150-152`):
```cpp
void Inventory::load(Serialize& s, Interactive&, World& w) {
  implLoad(nullptr,w,s);
  }
```
NEW:
```cpp
void Inventory::load(Serialize& s, Interactive& owner, World& w) {
  implLoad(nullptr,w,s);
  updateView(owner,w);
  }
```

## Risk
Low. Mirrors the existing NPC `mdlSlots` restore exactly, only adds a re-render for mobsi
inventories on load. `Interactive::setSlotItem` and `World::addView(const zenkit::IItem&)`
already exist and are used by the live placement path (`moveItem`). No save-format change.
