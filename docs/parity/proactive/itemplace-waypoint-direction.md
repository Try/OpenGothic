# Item spawned at waypoint ignores waypoint direction

> DEFER: visual-only (item yaw); the fix touches Item::updateMatrix for all items, so a wrong rotation would mis-render every world item. Needs a visual check before applying.

**Confidence:** Medium

## Original behaviour (Gothic2.exe)
When the script external `Wld_InsertItem(item, "WAYPOINT")` inserts an item, the
created `oCItem` vob is placed at the spawnpoint with the spawnpoint's full
transform, i.e. it inherits the waypoint/freepoint orientation (heading), not just
its position. Item-construction paths (`oCItem::InitByScript` @ 0x711bd0,
`oCItem::ThisVobAddedToWorld` @ 0x712df0) do not strip or reset that rotation; the
inserted vob keeps the rotation matrix it was given. So a sword/torch/etc. dropped
at a waypoint faces the waypoint heading.

## OpenGothic divergence
`game/world/worldobjects.cpp:660` `WorldObjects::addItem(inst,pos,dir)` correctly
fetches the waypoint direction (`worldobjects.cpp:649  dir = waypoint->direction();`)
and forwards it:

`game/world/worldobjects.cpp:670`
```cpp
it->setDirection(dir.x, dir.y, dir.z);
```

But `game/world/objects/item.cpp:147` is a no-op:
```cpp
void Item::setDirection(float, float, float) {
  }
```
and `Item::updateMatrix()` (`item.cpp:379`) builds an identity+translate matrix with
no rotation. Result: every waypoint-spawned world item is placed with identity
orientation, discarding the waypoint heading the caller deliberately passed in.
Gameplay-visible as mis-oriented placed items (purely rotational; position is
correct).

## Proposed patch

`game/world/objects/item.h` — add a yaw field:
```cpp
// OLD
    uint32_t                       amount   = 0;
// NEW
    uint32_t                       amount   = 0;
    float                          rotY     = 0; // yaw applied on world placement
```

`game/world/objects/item.cpp` — store heading and apply it in the matrix:
```cpp
// OLD
void Item::setDirection(float, float, float) {
  }
// NEW
void Item::setDirection(float x, float, float z) {
  // NOTE: in original-game Wld_InsertItem keeps the spawnpoint's orientation;
  // OpenGothic dropped it because setDirection was a no-op.
  if(x!=0.f || z!=0.f)
    rotY = 180.f*std::atan2(z,x)/float(M_PI);
  updateMatrix();
  }
```
```cpp
// OLD
void Item::updateMatrix() {
  Tempest::Matrix4x4 mat;
  mat.identity();
  mat.translate(pos.x,pos.y,pos.z);
  setLocalTransform(mat);
  }
// NEW
void Item::updateMatrix() {
  Tempest::Matrix4x4 mat;
  mat.identity();
  mat.translate(pos.x,pos.y,pos.z);
  mat.rotateOY(rotY); // NOTE: in original-game the inserted item keeps waypoint heading
  setLocalTransform(mat);
  }
```
(`<cmath>`/`M_PI` already pulled in transitively; `angleDir` in npc.cpp uses the same
`atan2(z,x)` convention, and `rotateOY` is used throughout, so the basis matches.)

Note: `setObjMatrix` (used by `addItemDyn` drops) already carries full rotation and is
untouched, so this only affects the static `Wld_InsertItem` waypoint path.
