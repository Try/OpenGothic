# zCVobLight: OpenGothic animates light RANGE per-frame, original Gothic2 animates only COLOR

**Confidence:** Medium (the divergence itself is well-evidenced across 3 independent original-engine read paths; the proposed patch is parity-correct but removes a visible OpenGothic feature, so it is gated on agreement that strict parity is wanted).

## Original function + address (prose only)

The only per-frame light animator in the original engine is `zCVobLight::DoAnimation`
(`0x006081c0`), called once per visible light from `zCBspTree::RenderLightList`
(`0x0052cfa0`) and `zCBspTree::Render` (`0x00530080`). Reconstructing the object
layout from `zCVobLight::SetByPresetInUse` (`0x00608e60`) and `zCVobLight::SetRange`
(`0x00608320`):

- `+0x120 / +0x128` = the range-animation keyframe array (`zCArray<float>`, the
  `rangeAniScale` list) and its element count.
- `+0x12c / +0x134` = the color-animation keyframe array (`zCArray<zCOLOR>`) and its
  count.
- `+0x144` = the light's *current effective range* (what the renderer reads).
- `+0x14c` = the light's *base* range (restored on `OnTrigger`, `0x00608050`).
- `+0x158 / +0x15c` = the color-animation cursor (phase) and color-animation fps.
- `+0x140` = the current animated color.

`DoAnimation` is gated on the COLOR fps and COLOR count (`+0x15c > 0 && +0x134 > 0`),
reads the color array at `+0x12c`, interpolates two neighbouring color keyframes into
`+0x140`, advances the color cursor `+0x158` by `frameTime * colorFps`, and wraps it
modulo the color count. It never touches `+0x144` (range) nor the range-animation array
at `+0x120`. The effective range `+0x144` is written only by `SetRange`, `SetByPresetInUse`
(once, from preset base `+0x48`), and `OnTrigger`/`OnUntrigger` (full base / 0). Every
runtime consumer of the range reads the static `+0x144`: `zCVobLight::SumLightsAtPositionWS`
(`0x00608410`) and `zCDynVobLightCacheElement::UpdateContents` (`0x005d6a10`). The
`rangeAniScale`/`rangeAniFPS` data is loaded and saved but is never applied to the light's
size at runtime — in the original, light radius does **not** pulse; only color flickers.

## OpenGothic file:line

`game/graphics/lightgroup.cpp:140-144` (range-animation wiring in `LightGroup::add`) and
`game/graphics/lightsource.cpp:107-123` (`LightSource::update`, the range-anim interpolation
that has no counterpart in the original).

## Divergence

`LightGroup::add` feeds `vob.range_animation_scale / range_animation_fps` into
`LightSource::setRange(arr, base, fps, smooth)`, and `LightSource::update` interpolates the
range keyframes into `curRgn` every tick. Consequences for any preset whose
`range_animation_scale` has more than one entry (e.g. flickering torch/fire presets):

1. The light's radius pulses each frame in OpenGothic, whereas in Gothic2.exe it stays
   fixed at the base range.
2. Even the steady/max range differs: OpenGothic seeds `rgn = max(scale_i * base)`
   (`lightsource.cpp:96-100`), which exceeds `base` whenever any scale factor is > 1, so
   the light reaches farther and its cull/bbox extent is larger than the original's
   `base`-derived range.

This is a runtime LOGIC divergence (an animation loop the original never runs), not a pure
color-shade difference, hence reported rather than silently dropped.

## Proposed patch

**DEFERRED for application, documented for decision.** A surgical, build-verifiable change
that restores original behaviour is:

```cpp
// game/graphics/lightgroup.cpp, LightGroup::add()
// OLD:
  if(!vob.range_animation_scale.empty()) {
    l.setRange(vob.range_animation_scale,vob.range,vob.range_animation_fps,vob.range_animation_smooth);
    } else {
    l.setRange(vob.range);
    }
// NEW:
  // NOTE: in original-game zCVobLight::DoAnimation @0x006081c0 only the color list is
  // animated per-frame; the effective range (+0x144) is written once from the preset base
  // and never from rangeAniScale (confirmed by SumLightsAtPositionWS @0x00608410 and
  // zCDynVobLightCacheElement::UpdateContents @0x005d6a10, which read the static range).
  l.setRange(vob.range);
```

Reason for DEFERRED rather than auto-fix: (a) this removes a visible OpenGothic feature
(pulsing-radius lights), so it should be a deliberate parity decision, not a silent revert;
(b) although three independent original read-paths were confirmed to use the static range,
the fixed-function D3D realtime-light submission path was not exhaustively traced, leaving a
small residual chance of an untraced range-anim application. If strict parity is the goal,
the patch above is correct and minimal; the color-animation path
(`vob.color_animation_*`, `lightsource.cpp:107` color branch) should be left untouched, as
it matches `DoAnimation` exactly.
