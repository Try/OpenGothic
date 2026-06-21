# MESH_SWAP (DEF_SWAPMESH) animation event is a no-op

> DEFER: needs a new node-to-node visual moveSlot helper (mdlvisual); DEF_SWAPMESH is a niche per-animation event. Defer pending the helper + a visual check.

**Confidence:** Medium

## Original behaviour
`oCNpc::DoModelSwapMesh(zSTRING from, zSTRING to)` at `0x00743dc0` (dispatched from
`oCNpc::DoDoAniEvents` at `0x00742a20`, which matches the ani-event name `DEF_SWAPMESH`
and calls vtable entry `+0xdc` with the event's two string args). The function:
- looks up the npc slot named by the first arg (`from`) in the npc slot list,
- looks up the npc slot named by the second arg (`to`),
- detaches the visual/vob held by each slot and re-attaches them swapped
  (`PutInSlot(fromSlot, toVob)`, `PutInSlot(toSlot, fromVob)`).

Net effect: the meshes (item visuals) attached to two named bone slots are exchanged.
This is used by interaction animations that visually swap a held mesh between nodes
(e.g. forge/anvil and similar `T_*` mob interaction sequences). `DEF_SWAPMESH` carries
two slot names, never an item instance.

## OpenGothic divergence
The event is parsed correctly but never acted upon.

- Parse: `game/graphics/mesh/animation.cpp:496-504` reads `slot[0]=e.slot`,
  `slot[1]=e.slot2` into an `EvTimed` and queues it.
- Handle: `game/world/objects/npc.cpp:2302-2303`
  ```cpp
  case zenkit::MdsEventType::MESH_SWAP:
    break;
  ```
  The case does nothing, so the two slot names are discarded and no visual swap occurs.

OpenGothic attaches item view-meshes to named bone slots via `Npc::setSlotItem` /
`Npc::clearSlotItem`, tracked in `Inventory::mdlSlots` (`game/game/inventory.cpp:712`,
`735`). A swap is therefore expressible in OG's model: move the `mdlSlots` entry keyed
on `slot[0]` to `slot[1]` (and re-attach the view there). The original swaps both
directions; in practice DEF_SWAPMESH source slots typically hold one occupied + one
empty node, so a one-directional move covers the common case. A full two-way swap can
be added if both slots are found occupied.

## Proposed patch
Minimal one-directional move of the visual from `slot[0]` to `slot[1]`, mirroring the
original node-to-node mesh transfer.

File: `game/world/objects/npc.cpp`
```cpp
      case zenkit::MdsEventType::MESH_SWAP:
        break;
```
NEW:
```cpp
      case zenkit::MdsEventType::MESH_SWAP:
        // NOTE: in original-game oCNpc::DoModelSwapMesh (0x00743dc0) DEF_SWAPMESH swaps
        // the item visuals attached to two named node slots (slot[0] <-> slot[1]).
        invent.moveSlot(*this, i.slot[0], i.slot[1]);
        break;
```

File: `game/game/inventory.h` (declare next to `putToSlot`)
```cpp
    void   putToSlot       (Npc& owner, size_t cls, std::string_view slot);
```
NEW (add line after it):
```cpp
    void   putToSlot       (Npc& owner, size_t cls, std::string_view slot);
    void   moveSlot        (Npc& owner, std::string_view from, std::string_view to);
```

File: `game/game/inventory.cpp` (implement after `putToSlot`)
```cpp
void Inventory::moveSlot(Npc& owner, std::string_view from, std::string_view to) {
  // NOTE: in original-game DEF_SWAPMESH moves the held visual from one node slot to another.
  if(from.empty() || to.empty() || from==to)
    return;
  for(auto& i:mdlSlots)
    if(i.slot==from && i.item!=nullptr) {
      const size_t cls = i.item->clsId();
      clearSlot(owner,from,false);
      putToSlot(owner,cls,to);
      return;
      }
  }
```

## Gameplay effect
Without the fix, animations that swap a held item visual between two bones leave the
mesh on the original node (or invisible), instead of moving it as the original engine
does. Niche (mob-interaction T_-anis), hence Medium confidence; the parse already
preserves both slot names, so the fix is contained.
