# Death-drop: in-hand items keep hand-bone rotation instead of being world-axis-reset

**Confidence:** High

## Original function + address

When an NPC goes down, `oCNpc::DoDie` @0x00736760 (lethal) and `oCNpc::DropUnconscious`
@0x00735eb0 (fist-KO) both call `oCNpc::DropAllInHand` @0x007375e0. `DropAllInHand` drops
the two occupied hand-node slots (right hand = drawn weapon, left hand = shield/torch) by
calling `oCNpc::DropFromSlot` @0x0074a660 for each.

`DropFromSlot` turns the held item into a free world vob by constructing an `oCAIVobMove`
and invoking `oCAIVobMove::Init` @0x0069f540. Inside `Init`, before the throw physics are
armed, the dropped vob is repositioned via:

- `zCVob::ResetRotationsWorld` @0x0061c000 — resets the vob's world rotation to
  axis-aligned (identity), and
- `zCVob::SetPositionWorld` — sets only the translation (taken from the NPC trafo).

So in the original, a weapon/shield/torch dropped on death/KO lands **world-axis-aligned**
(flat), not tilted at the hand-bone orientation. This is the exact same mechanism already
mirrored for the scripted `Npc_DropItem` path: see the existing NOTE at
`game/world/objects/npc.cpp:3761` (`Npc::dropItem`, citing `oCNpc::DoDropVob` @0x00744dd0
→ same `oCAIVobMove::Init` → `ResetRotationsWorld`), which builds an identity matrix and
copies only the translation.

## OG file:line (divergence sites)

The death-drop path was **not** given the same rotation reset. All three pass the raw
hand-bone matrix (rotation + translation) straight into `World::addItemDyn`:

- `game/graphics/mdlvisual.cpp:293` — `MdlVisual::dropWeapon`: `p = pose.bone(att->boneId)`
  passed to `addItemDyn(itm->clsId(), p, ...)`.
- `game/graphics/mdlvisual.cpp:314` — `MdlVisual::dropShield`: same, `addItemDyn(..., p, ...)`.
- `game/world/objects/npc.cpp:883` — `Npc::dropTorch`: `mat = visual.pose().bone(leftHand)`
  passed to `addItemDyn(torchId, mat, ...)`.

## Divergence

In OpenGothic, a weapon/shield/torch dropped when an NPC dies or is knocked unconscious
inherits the full hand-bone rotation, so it spawns tilted/embedded at the hand's
orientation. The original resets the dropped vob's rotation to world-axis-aligned
(`ResetRotationsWorld`) and keeps only the position, so it lies flat. `Npc::dropItem`
(scripted drop) already reproduces this; the death-drop trio is the un-fixed sibling path
of the identical original mechanism (`oCAIVobMove::Init`).

## Proposed patch (OLD/NEW)

Mirror the established `Npc::dropItem` fix at all three death-drop sites: keep the bone
translation, discard the bone rotation.

### `game/graphics/mdlvisual.cpp` — `MdlVisual::dropWeapon`
OLD:
```cpp
  auto it = npc.world().addItemDyn(itm->clsId(),p,npc.handle().symbol_index());
  it->setCount(1);
```
NEW:
```cpp
  // NOTE: in original-game oCNpc::DropAllInHand @0x007375e0 -> oCNpc::DropFromSlot
  // @0x0074a660 -> oCAIVobMove::Init @0x0069f540 the dropped in-hand vob's rotation is
  // reset to world-axis-aligned via zCVob::ResetRotationsWorld @0x0061c000 (only the
  // position survives) before it falls. Drop the hand-bone rotation, matching Npc::dropItem.
  Tempest::Matrix4x4 drop;
  drop.identity();
  drop.translate(p.at(3,0),p.at(3,1),p.at(3,2));
  auto it = npc.world().addItemDyn(itm->clsId(),drop,npc.handle().symbol_index());
  it->setCount(1);
```

### `game/graphics/mdlvisual.cpp` — `MdlVisual::dropShield`
OLD:
```cpp
  auto it = npc.world().addItemDyn(itm->clsId(),p,npc.handle().symbol_index());
  it->setCount(1);
```
NEW:
```cpp
  // NOTE: in original-game oCNpc::DropFromSlot @0x0074a660 -> oCAIVobMove::Init @0x0069f540
  // resets the dropped in-hand vob's rotation via zCVob::ResetRotationsWorld @0x0061c000.
  Tempest::Matrix4x4 drop;
  drop.identity();
  drop.translate(p.at(3,0),p.at(3,1),p.at(3,2));
  auto it = npc.world().addItemDyn(itm->clsId(),drop,npc.handle().symbol_index());
  it->setCount(1);
```

### `game/world/objects/npc.cpp` — `Npc::dropTorch`
OLD:
```cpp
      auto mat = visual.transform();
      if(leftHand<visual.pose().boneCount())
        mat = visual.pose().bone(leftHand);

      owner.addItemDyn(torchId,mat,hnpc->symbol_index());
```
NEW:
```cpp
      auto mat = visual.transform();
      if(leftHand<visual.pose().boneCount())
        mat = visual.pose().bone(leftHand);

      // NOTE: in original-game oCNpc::DropFromSlot @0x0074a660 -> oCAIVobMove::Init
      // @0x0069f540 resets the dropped torch's rotation via zCVob::ResetRotationsWorld
      // @0x0061c000, keeping only the position; mirror Npc::dropItem.
      Tempest::Matrix4x4 drop;
      drop.identity();
      drop.translate(mat.at(3,0),mat.at(3,1),mat.at(3,2));
      owner.addItemDyn(torchId,drop,hnpc->symbol_index());
```

### Lower-confidence aside (NOT patched)
`DropFromSlot` actually feeds `oCAIVobMove::Init` the translation from
`zCVob::GetTrafoModelNodeToWorld` of the NPC vob (≈ NPC root position) rather than the hand
bone, then throws the item with force 100.0f. OpenGothic uses the hand-bone translation and
no throw. The exact start position is largely washed out by the original's throw-and-settle,
so only the rotation reset is proposed here as the high-confidence, precedent-backed fix
(identical to the already-applied `Npc::dropItem` behavior).
