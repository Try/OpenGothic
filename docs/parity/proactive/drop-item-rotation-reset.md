# Dropped item keeps right-hand bone rotation instead of resetting to world-axis-aligned

**Confidence:** High

## Original function + address

`oCNpc::DoDropVob` (Gothic2.exe `0x00744dd0`) computes the drop position and
then constructs an `oCAIVobMove` AI and calls `oCAIVobMove::Init`
(`0x0069f540`) with the item vob, the dropping NPC, the drop position, a speed
of `0.0`, a throw magnitude of `100.0` (the float constant `0x42c80000`), and
the NPC's world trafo matrix.

Inside `oCAIVobMove::Init`, immediately before placing the item, the original
calls `zCVob::ResetRotationsWorld` (`0x0061c000`) on the item. That routine
overwrites the vob's 3x3 rotation sub-matrix with the identity matrix
(`DAT_008d45e8`) while saving and restoring only the translation column
(offsets `0x50/0x60/0x70`). The sequence is:
`SetCollDet... -> ResetRotationsWorld(item) -> SetPositionWorld(item, dropPos)
-> SetSleeping(item,0) -> SetPhysicsEnabled(item,1) -> SetVelocity(...)`.

Net effect: a dropped item enters the physics simulation **world-axis-aligned
(identity rotation)**. Bullet/rigid-body settling then tilts it as it falls and
hits the floor; it does not start rotated to match whatever pose the right hand
happens to be in.

## OpenGothic file:line

`game/world/objects/npc.cpp:3551-3555` (`Npc::dropItem`):

```cpp
auto mat = visual.transform();
if(rightHand<visual.pose().boneCount())
  mat = visual.pose().bone(rightHand);

auto it = owner.addItemDyn(id,mat,hnpc->symbol_index());
```

`addItemDyn` (`game/world/worldobjects.cpp:675`) feeds this matrix straight into
`Item::setObjMatrix`, and `Item::setPhysicsEnable` builds the dynamic body from
`transform()` — so the full hand-bone matrix, including its arbitrary
animation-driven rotation, becomes the item's initial physics transform.

## Divergence

OpenGothic seeds the dropped item's physics transform with the **rotation of the
`ZS_RIGHTHAND` bone**, whereas the original resets the item's rotation to
identity (world-aligned) before enabling physics. As a result, OG items can
start the drop tumble at an arbitrary canted orientation taken from the hand
pose, while the original always starts them upright/axis-aligned. The position
column is the same in both, so only the on-ground orientation diverges.

## Proposed patch

Strip the rotation from the placement matrix, keeping only the translation, to
mirror `ResetRotationsWorld`. The identity+translate idiom is already used in
this file's `Item::updateMatrix` (`game/world/objects/item.cpp:382-383`), and
`addItemDyn`/`setObjMatrix` already accept a `Tempest::Matrix4x4`.

OLD (`game/world/objects/npc.cpp:3551-3555`):
```cpp
  auto mat = visual.transform();
  if(rightHand<visual.pose().boneCount())
    mat = visual.pose().bone(rightHand);

  auto it = owner.addItemDyn(id,mat,hnpc->symbol_index());
```

NEW:
```cpp
  auto mat = visual.transform();
  if(rightHand<visual.pose().boneCount())
    mat = visual.pose().bone(rightHand);

  // NOTE: in original-game oCAIVobMove::Init @0x0069f540 (called from
  // oCNpc::DoDropVob @0x00744dd0) the dropped item's rotation is reset to
  // identity via zCVob::ResetRotationsWorld @0x0061c000 before physics is
  // enabled; only the world position is taken from the hand. Keep translation,
  // drop the hand-bone rotation so the item starts world-axis-aligned.
  Tempest::Matrix4x4 drop;
  drop.identity();
  drop.translate(mat.at(3,0),mat.at(3,1),mat.at(3,2));

  auto it = owner.addItemDyn(id,drop,hnpc->symbol_index());
```

Verified OG symbols: `Tempest::Matrix4x4::identity()`, `::translate()`, and
`::at(int,int)` are all used in `game/world/objects/item.cpp` (lines 382-383,
151-153); `WorldObjects::addItemDyn(size_t,const Tempest::Matrix4x4&,size_t)`
exists at `game/world/worldobjects.cpp:675`.

Scope note: this fixes only the initial orientation. The original additionally
imparts a forward throw velocity (~100 units, the `0x42c80000` argument) via the
rigid body, which OpenGothic's `DynamicWorld::Item` has no API to set; that
throw-arc gap is DEFERRED (would require new physics-body velocity plumbing).
