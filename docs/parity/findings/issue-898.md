# Issue #898 — Blinking texture

- **Category:** render-bug
- **Disposition:** DEFER (needs runtime repro with attached save to localize)

## Issue
Reporter (mztulip, label: bug) sees a texture blinking/flickering, most visible on the
path to a temple and when entering/exiting water; possibly elsewhere. Video + repro zip
(`blinking_texture.zip`) attached. No stack trace or asset name in the body.

## OG files (candidate surfaces)
- `game/graphics/material.cpp:13` — `Material::Material` (alpha-func resolution, animated
  texture frames, `texAniMapDir` periods, wave mode)
- `game/graphics/material.cpp:47` — decal material ctor (`alpha_func`, `alpha_weight`)
- `shader/scene.glsl`, `shader/water/*` — terrain/water shading
- `game/graphics/drawcommands.cpp`, `drawbuckets.cpp` — material bucketing / draw order
- `game/graphics/landscape.cpp` — terrain submesh assembly

## Original behavior (prose)
ZenGin renders overlapping terrain/decal/water surfaces with stable DX7 polygon ordering
and a fixed depth bias for decals, so coincident surfaces (e.g. a path texture laid over
terrain, or the water plane meeting the shore) do not z-fight. Animated textures advance on
a fixed timestep without dropping/duplicating frames.

## Likely divergence (hypotheses to confirm at runtime)
"Blinking" on a terrain path and at water edges is the classic signature of one of:
1. **Z-fighting** between two coplanar surfaces (path decal vs terrain, or water plane vs
   shore) — OG may lack/ misapply the original decal depth-bias, so visibility flips per
   frame as the camera moves.
2. **Material alpha-mode mismatch** — `loadAlphaFunc` (material.cpp:26/53) classifying a
   surface as alpha-tested vs blended differently from vanilla, causing intermittent
   discard at the alpha threshold along the shore/waterline.
3. **Animated-texture frame timing** (`texAniMapDirPeriod`, `texAniFPSInv`) producing a
   degenerate period that flips frames erratically.

## Why DEFER
Cannot confirm a surgical fix without loading the attached save and identifying the exact
material/VOB that flickers (need the texture name + whether it is terrain, a `zCDecal`, or
the water plane). The fix differs entirely per root cause (depth bias vs alpha-func vs
ani-timing). This requires runtime reproduction, which is out of scope here.

## Investigation guidance
- Load `blinking_texture.zip` save, walk the temple path / water edge, capture a frame and
  read the flickering material's name + group + `alpha_func`.
- If two surfaces fight: verify decal depth-bias handling in `drawcommands.cpp` /
  pipeline depth state vs the original's decal offset.
- If waterline: check `Material` Water-vs-Solid classification (material.cpp:27 has a
  waterfall heuristic — confirm a similar shore/water classification gap).
