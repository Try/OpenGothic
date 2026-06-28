# C_Item.inv_animate: inventory 3D-preview spin flag parsed but never animated

**Confidence:** High (parsed-but-ignored is certain; patch faithfulness is the only caveat)

## Original fn + address (prose)

The script field `C_Item.inv_animate` is mirrored into the engine object `oCItem`
at member offset `+0x328` by `oCItem::InitByScript` @ `0x00711bd0` (the parser
instance block is copied verbatim into the vob; the last five C_Item ints land at
`+0x318 inv_zbias`, `+0x31c inv_rot_x`, `+0x320 inv_rot_y`, `+0x324 inv_rot_z`,
`+0x328 inv_animate`).

`oCItem::RotateInInventory` @ `0x007132e0` is gated entirely by
`*(int*)(this+0x328) != 0`: when `inv_animate` is non-zero it picks the local axis
along the item's **largest bounding-box extent** and calls `zCVob::RotateLocal` by
`frameTime_ms * 0.02` degrees every frame (the constant `0x3ca3d70a` == `0.02f`),
i.e. an accumulating continuous spin (~20 deg/s). When `inv_animate == 0` the
function is a no-op.

It is invoked from `oCItem::RenderItem` @ `0x00713ac0`: that function calls
`RotateInInventory(this)` when its rotation-angle argument is `0.0`, otherwise it
calls `RotateForInventory` (the static `inv_rot_x/y/z` orientation OG already
implements). The big item-info 3D preview pane (`oCItemContainer::DrawItemInfo`
@ `0x00706e40`) always passes `0.0`, so the previewed item spins iff `inv_animate`
is set.

## OG file:line

- Parsed/serialized only: `game/world/objects/item.cpp:58` and `:128`
  (`fin.read(... h->inv_animate)` / `fout.write(... h.inv_animate)`).
- Inventory 3D rendering that consumes the other `inv_*` fields but **not**
  `inv_animate`: `game/graphics/inventoryrenderer.cpp:51-130`
  (`InventoryRenderer::drawItem` applies `inv_rot_x/y/z` and `inv_zbias` to a
  per-frame `viewMat`, but never a time-based spin).
- Grep confirms `inv_animate` appears nowhere in `game/` outside the two
  serialization lines above.

## Divergence

OpenGothic loads `C_Item.inv_animate` and round-trips it through save/load, but no
code path ever rotates the rendered inventory mesh over time. In the original
engine, items authored with `inv_animate == 1` slowly spin in the inventory 3D
preview (and in slots rendered with the spin path). In OG those items render with
a fixed orientation, so the cosmetic "rotating item" feedback is missing.

## Proposed patch

`InventoryRenderer::drawItem` rebuilds each item's `viewMat` every frame, so a
continuous spin can be layered onto the existing static rotation using the global
wall clock (`Tempest::Application::tickCount()`, already used in
`game/camera.cpp:649` and `game/mainwindow.cpp`). The original's per-frame step is
`frameTime_ms * 0.02 deg`, whose steady-state angle equals `tickCount_ms * 0.02 deg`.

`game/graphics/inventoryrenderer.cpp`, in `drawItem`, after the category `roty`
base rotation is established (around line 69-76, where `roty` is read):

OLD:
```cpp
    float rotx = float(itData.inv_rot_x);
    float roty = float(itData.inv_rot_y);
    float rotz = float(itData.inv_rot_z);
```
NEW:
```cpp
    float rotx = float(itData.inv_rot_x);
    float roty = float(itData.inv_rot_y);
    float rotz = float(itData.inv_rot_z);

    // NOTE: in original-game oCItem::RotateInInventory @0x007132e0 a non-zero
    // C_Item.inv_animate (oCItem+0x328) makes the inventory preview spin by
    // frameTime_ms*0.02 deg/frame; OpenGothic parsed inv_animate but never
    // animated it. Approximate the steady-state spin with the global clock.
    if(itData.inv_animate!=0)
      roty += float(Tempest::Application::tickCount()%18000)*0.02f;
```
(Requires `#include <Tempest/Application>` at the top of the file.)

Caveat / not-exact: the original spins around the item's largest bbox axis and, on
its vob-based path, the spin is mutually exclusive with the static category
orientation (`RotateInInventory` vs `RotateForInventory`). OG's `InventoryRenderer`
always applies the static category orientation, so the patch above adds the spin
around the vertical (Y) axis on top of that orientation rather than reproducing the
original's exclusive longest-axis path. This restores the user-visible
"inv_animate items rotate" behavior surgically in one file; an exact axis/path
match would require reworking the renderer's orientation pipeline and is left out.
