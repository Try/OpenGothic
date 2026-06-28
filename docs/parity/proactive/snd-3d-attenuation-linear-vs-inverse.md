# snd-3d-attenuation-linear-vs-inverse

3D sound distance falloff: OpenGothic uses a linear gain ramp to zero at the
sound radius, whereas the original ZenGin (Miles Sound System) uses an
inverse-distance (1/d) rolloff with a per-source inner "min distance".

**Confidence:** High that the divergence exists. **Fix: DEFERRED** (no surgical,
high-confidence, in-`game/` fix — see reason below).

## Original function + address (prose)

- `zCSndSys_MSS::PlaySound3D(zCSoundFX*, zCVob*, int, zTSound3DParams*)` at
  **Gothic2.exe 0x004f10f0** configures every 3D voice with
  `AIL_set_3D_sample_distances(sample, RANGE_SCALE * maxDist, RANGE_SCALE * minDist)`.
  `maxDist` is the vob radius (`zTSound3DParams.radius`, or the global default
  when the field is the `-1.0` sentinel). `minDist` is *not* zero: it is derived
  per source from the emitter's bounding-box half-extents via a fast octagonal
  length approximation (`(min+mid)*0.375 + max*0.9375`) and then projected so the
  inner full-volume radius scales with the object's size.
- `zCSndSys_MSS::InitializeMSS` at **0x004eb830** never overrides the Miles 3D
  rolloff model, so the engine uses Miles' default **inverse-distance** law:
  full volume within `minDist`, then gain `= minDist / distance`, clamped to
  silence past `maxDist`. The curve is convex (loud across most of the radius,
  steep drop only near the source for small `minDist`), not linear.
- The global default radius is confirmed 3500 units: `zCSndSys_MSS::zCSndSys_MSS`
  at **0x004eaaf0** stores `0x455ac000` (= 3500.0f) into `zCSoundSystem+4`
  (`GetSound3DDefaultRadius`, 0x004eb3b0). OpenGothic's `Sound` ctor default of
  `3500.f` matches this, so the *radius* itself is faithful — only the *curve*
  between 0 and the radius diverges.

## OpenGothic file:line

- `lib/Tempest/Engine/sound/sounddevice.cpp:173` —
  `alDistanceModelDirect(data->context, AL_LINEAR_DISTANCE);`
- `lib/Tempest/Engine/sound/soundeffect.cpp:46` and `:69` —
  `alSourcefDirect(ctx, source, AL_REFERENCE_DISTANCE, 0);`
- `game/world/worldsound.cpp:154-158` (`WorldSound::implAddSound`) —
  `eff.setMaxDistance(rangeMax);` with `rangeMax` = the vob radius / 3500 default.

## Divergence

With OpenAL's `AL_LINEAR_DISTANCE` model, reference distance `0`, rolloff `1`,
and max distance = radius, the source gain is:

```
gain = 1 - clamp(d, 0, radius) / radius          // linear, reaches 0 at radius
```

The original Miles inverse-distance model with a non-zero `minDist` gives:

```
gain = minDist / max(d, minDist)                 // 1/d, full volume within minDist
```

Concrete consequence: at half the radius (`d = radius/2`) OpenGothic plays the
sound at gain 0.5, while the original (with, say, `minDist` a few hundred units)
plays it at roughly `minDist/(radius/2)` — typically much *louder* in the
near/mid field, then dropping more sharply only close to the emitter. OpenGothic
3D emitters therefore fade out far too early and too uniformly with distance, and
the inner full-volume plateau (`minDist`) is absent entirely (reference distance
is pinned to 0). This is the "linear vs the original's falloff" attenuation-curve
divergence.

## Proposed patch — DEFERRED

A faithful fix is **not** a surgical, build-verifiable one-liner in `game/`:

1. The distance model and reference distance live in the Tempest engine submodule
   (`lib/Tempest/Engine/sound/`), outside the `game/` parity surface. Switching to
   `AL_INVERSE_DISTANCE_CLAMPED` would change attenuation for *all* sounds
   (UI/2D, dialogue, footsteps) and interacts with `AL_METERS_PER_UNIT = 100`
   and the existing per-effect volume/occlusion math, risking broad regressions.
2. The original's behavior also depends on a per-source `minDist` reconstructed
   from the emitter bounding box (PlaySound3D @0x004f10f0) plus the `RANGE_SCALE`
   unit factor; reproducing it 1:1 requires reverse-engineering and re-deriving
   that bbox formula and feeding a reference distance per voice — well beyond a
   single high-confidence constant/formula edit.

Because "empty beats false positives" and no surgical in-scope fix is available,
this is recorded as a documented divergence only.

```
// NOTE: in original-game zCSndSys_MSS::PlaySound3D @0x004f10f0 the 3D rolloff is
// Miles' inverse-distance law (gain = minDist/dist, full volume within a per-vob
// bbox-derived minDist, clamped at the radius); InitializeMSS @0x004eb830 leaves
// the default model in place. OpenGothic uses AL_LINEAR_DISTANCE
// (sounddevice.cpp:173) with AL_REFERENCE_DISTANCE 0 (soundeffect.cpp:46,69),
// i.e. gain = 1 - dist/radius, so 3D emitters fade linearly and lack the
// near-field full-volume plateau. DEFERRED: faithful fix is engine-level + needs
// the bbox minDist reconstruction, not a surgical game/ constant edit.
```
