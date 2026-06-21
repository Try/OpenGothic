# Issue #901 — VFX events in animations (lighting)

- **Category:** animation / visual-fx
- **Disposition:** DEFER (architectural; owner-filed tracking issue)

## Issue
Owner-filed (Try). Tracks a lighting problem related to VFX spawned from animation events,
referencing discussion #850 and noted as "probably same general-issue" as #858. The
discussion centers on dynamic-light behavior: NPCs/hero not casting shadows from
torch/candle point-lights, sun-shadow edge flicker, and over-bright torch/candle output
(see also #871).

## OG files
- `game/graphics/mesh/animation.cpp:375` — `Animation::Sequence::processPfx` (frame-window
  extraction → spawns/stops effect per `MdsParticleEffect`)
- `game/graphics/mesh/pose.cpp:576` — `Pose::processPfx`
- `game/graphics/mdlvisual.cpp:320` — `MdlVisual::startEffect`; `:351/:362` `stopEffect`
- `game/graphics/visualfx.cpp:41-42, 103` — VFX `light_preset_name` / `light_range`
- `game/graphics/lightsource.cpp`, `lightgroup.cpp`, `renderer.cpp` — light/exposure path

## Original behavior (prose)
In ZenGin, animation pfx/sfx events (`*eventPFX*` / `*eventSFX*` MDS tags) emit a
`zCVisualFX`/`zCParticleFX` at the keyed frame on the named bone; VFX carrying a
light-preset spawn a `zCVobLight` that contributes to the DX7 fixed-function light set.
Lights are unitless and clamped by the DX7 ~8-simultaneous-light limit, so vanilla
brightness values were authored against that pipeline.

## OG current / divergence
The pfx-event emission path itself is implemented (`processPfx` start/stop by slot). The
divergence is in **lighting/rendering of these effects**, which OG drives through a modern
HDR/exposure pipeline: per owner comments on #871, OG applies "hacks to bypass exposure"
for vob-lights, dynamic point-lights do not cast NPC shadows, and vanilla brightness values
"ruin the rendering" under the new pipeline. The owner explicitly defers this pending a
particles-rendering rework blocked by a "camera issue".

## Why DEFER (no surgical patch)
This is an owner-authored umbrella issue requiring renderer/light-pipeline rework
(exposure handling for vob/VFX lights, dynamic-light shadow casting, particle camera fix).
There is no single confirmable line-level divergence vs the original; any fix is a design
change, not a parity bug-fix. Recommend tracking alongside #858 and #871.

## Investigation guidance
- Audit how VFX `lightPresetName`/`lightRange` (visualfx.cpp:41-42) feed `lightsource.cpp`
  and whether their intensity is run through exposure in `renderer.cpp`.
- Compare against #871 light-exposure-bypass hacks; a unified exposure policy for
  authored (vanilla) lights vs HDR scene lights likely resolves both.
