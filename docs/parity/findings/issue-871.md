# Issue #871 — Magic fog around potions missing; light too bright

- **Category:** visual-fx
- **Disposition:** DEFER (architectural + partly intentional; owner-acknowledged)

## Issue
Two parts (label: bug):
1. The blue "magic fog"/star particle aura around potions present in the original is absent
   in OG (video provided).
2. Some light sources (and torches) render as over-bright white even at minimum brightness;
   repro save `potion_save_slot_5.zip` at commit f0ca41d.

## OG files
- `game/graphics/pfx/particlefx.cpp`, `pfxobjects.cpp`, `pfxbucket.cpp` — particle system
  (CPU-side emission/simulation)
- `game/graphics/visualfx.cpp` — VFX/light preset binding
- `game/graphics/lightsource.cpp`, `lightgroup.cpp` — world/vob lights
- `game/graphics/renderer.cpp`, `sceneglobals.cpp` — exposure / tone-mapping

## Original behavior (prose)
In ZenGin the potion aura is a `zCParticleFX` with a **mesh emitter** (particles seeded
across the item mesh surface), simulated on CPU. World/vob lights are unitless DX7
fixed-function lights authored to look correct under that fixed pipeline and its
~8-simultaneous-light cap; many vanilla lights exist purely for lightmap baking or are
invisible at runtime due to the light-count limit.

## Divergence (per owner comments, verbatim-paraphrased)
- **Magic fog:** OG would need to run a mesh-emitter particle system on CPU; the owner
  flags this as costly at large view range and not yet implemented. So the effect is
  *absent*, not mis-rendered. (Owner declined a sphere-emitter approximation as "too little
  to make a difference".)
- **Lighting:** OG runs a modern HDR/exposure pipeline; vob-lights currently use "hacks ...
  to make them bypass exposure". Vanilla brightness values, fed to the new pipeline, blow
  out to white. The owner suspects vanilla values "ruin the rendering" and that the old
  DX7 ~7-light limit silently dropped many lights the OG pipeline now renders.

## Why DEFER (no surgical patch)
- The missing magic fog requires implementing/optimizing a CPU mesh-emitter particle path —
  a feature addition, not a line-level parity fix.
- The over-bright lights stem from the exposure-bypass design choice and the absence of the
  original's light-count clamp; correcting it is a rendering-pipeline policy change (shared
  root with #901/#858), not a confirmable surgical edit.
Both are owner-acknowledged and explicitly deferred pending particle/light rework.

## Investigation guidance
- Magic fog: gate a mesh-emitter pfx by view-range/LOD so it only simulates near the
  camera (addresses the owner's perf concern); wire item-attached `zCParticleFX` emitters
  in `pfxobjects.cpp`.
- Lights: replace per-light exposure-bypass hacks with a consistent exposure policy for
  authored vanilla lights, and consider re-introducing an N-brightest-lights clamp to
  approximate the DX7 limit so vanilla scenes match.
