# C_Item.inv_zbias: inventory 3D-preview zoom parsed but never applied

**Confidence:** Medium-High (parsed-but-ignored is certain and grep-proven; the
field's meaning is read directly from the original; the only caveat is that
OpenGothic's flat-scale inventory renderer approximates the original's
perspective-camera distance, so the mapping is a faithful proportional
translation rather than a pixel-identical match.)

## Original fn + address (prose)

The script field `C_Item.inv_zbias` is mirrored into the engine object `oCItem`
at member offset `+0x318` (the last five C_Item ints land at `+0x318 inv_zbias`,
`+0x31c inv_rot_x`, `+0x320 inv_rot_y`, `+0x324 inv_rot_z`, `+0x328 inv_animate`,
per `oCItem::InitByScript`).

`oCItem::RenderItemPlaceCamera` (entry ~`0x00713800`, source tag
`oItem.cpp`, called from `oCItem::RenderItem` @ `0x00713ac0`) positions the
preview camera for the inventory item render. It first computes an **auto-fit**
camera distance from the item's local bounding box and the camera FOV:
`dist0 = sin(90 - fov/2) * bboxDiagonal / sin(fov/2)` — i.e. the distance that
frames the bbox in view (apparent on-screen size therefore scales as `1/dist`,
which is `~1/bboxDiagonal`). It then branches on `inv_zbias`:

- If `*(int*)(this+0x318) != 0` (`inv_zbias` set): `dist = inv_zbias * 0.01 * dist0`
  (the constant `0x3c23d70a` == `0.00999999978f` ≈ `1/100`). So `inv_zbias` is the
  **percent of the auto-fit distance**: `100` = neutral (same as auto-fit), `<100`
  pulls the camera in (item appears larger), `>100` pushes it back (item appears
  smaller). This branch then skips the per-category default zoom.
- If `inv_zbias == 0`: it falls through to a `main_flag` switch that applies
  per-category distance multipliers (1.35 / 1.45 / 1.5 ...).

A separate global `s_fGlobalItemZBiasScale` multiplies the result for every item
(a global tuning knob, not per-item).

## OG file:line

- Parsed/serialized only: `game/world/objects/item.cpp:58` (`fin.read(h->inv_zbias,...)`)
  and `:128` (`fout.write(h.inv_zbias,...)`).
- Inventory 3D render that consumes `inv_rot_x/y/z` but **not** `inv_zbias`:
  `game/graphics/inventoryrenderer.cpp:54-64,152` (`InventoryRenderer::drawItem`
  computes the auto-fit scale `sz = 2.f/bboxDiagonal`, clamps it, and applies
  `mat.scale(sz)`).
- Grep confirms `inv_zbias` appears nowhere in `game/` outside the two
  serialization lines above. (Note: the existing
  `docs/parity/proactive/flag-item-inv-animate.md` claims `drawItem` "applies
  `inv_rot_x/y/z` and `inv_zbias`" — that is incorrect; only the rotations are
  applied.)

## Divergence

OpenGothic loads `C_Item.inv_zbias` and round-trips it through save/load, but no
code path uses it. In the original engine, an item authored with e.g.
`inv_zbias = 80` is previewed 1/0.8 = 1.25x larger than the auto-fit default, and
`inv_zbias = 150` renders it 0.667x smaller — a per-item authored zoom used to
keep oversized or undersized item meshes legible in the inventory cell / the big
item-info preview pane. In OG every item is rendered at the plain auto-fit scale,
so the authored zoom is lost.

Because OG's auto-fit scale `sz = 2/bboxDiagonal` is proportional to `1/dist0`
(the same `1/bboxDiagonal` apparent-size law as the original's auto-fit camera),
the original's `dist = inv_zbias*0.01*dist0` maps cleanly to OG as a scale factor
`sz_final = sz / (inv_zbias*0.01) = sz * (100/inv_zbias)`.

## Proposed patch

`game/graphics/inventoryrenderer.cpp`, in `drawItem`, at the auto-fit scale
computation:

OLD:
```cpp
    sz = 2.f/sz;
    if(sz>0.1f)
      sz=0.1f;
```
NEW:
```cpp
    sz = 2.f/sz;
    // NOTE: in original-game oCItem::RenderItemPlaceCamera @~0x00713800 a non-zero
    // C_Item.inv_zbias (oCItem+0x318) overrides the auto-fit preview distance with
    // dist = inv_zbias*0.01*dist0 (const 0x3c23d70a == 0.01). OpenGothic's auto-fit
    // scale sz=2/bboxDiag is proportional to 1/dist0, so the authored zoom maps to
    // sz *= 100/inv_zbias (100 == neutral, <100 larger, >100 smaller). OpenGothic
    // parsed inv_zbias but never applied it, rendering every item at the neutral
    // auto-fit scale.
    if(itData.inv_zbias!=0)
      sz *= 100.f/float(itData.inv_zbias);
    if(sz>0.1f)
      sz=0.1f;
```

Caveat (does not block the fix): OG's `0.1f` scale clamp has no original
equivalent (the original sizes via a perspective camera distance), so extreme
`inv_zbias` values that push `sz` past the clamp will saturate; and the original's
per-category default multipliers for the `inv_zbias == 0` branch remain
unimplemented in OG (a separate, pre-existing gap). Applying `100/inv_zbias` to
the auto-fit scale is strictly closer to the original than ignoring the field and
matches the proportional `1/bboxDiagonal` apparent-size law both engines share at
the neutral setting.
