# PFX scale-key (pps emit-rate / shape) non-smooth keyframe sampling: nearest vs floor

**Confidence:** Medium-High

## Original function + address (prose only)

Two original routines drive the per-frame particle-FX scale keys:

- `zCParticleFX::GetNumParticlesThisFrame` @ `0x005b1a90` — computes how many
  particles to emit this frame from the pps scale-key animation.
- `zCParticleFX::GetShapeScaleThisFrame` @ `0x005b1920` — computes the emitter
  shape-scale this frame from the shape scale-key animation.

Both share the same sampling model. They keep a floating "key phase" accumulator
that is advanced each frame by `keyFPS * 0.001 * frameTimeMs`. The current key
index is `floor(phase)`. The sampled value is:

- when the key list is **not smooth**: simply `key[floor(phase)]` — a pure step
  function that holds one key for the whole integer key interval; it never looks
  at the next key and never rounds to the nearest key.
- when the key list is **smooth**: a linear interpolation between
  `key[floor(phase)]` and the next key (with the next index wrapping to 0 past the
  end), weighted by the fractional part `phase - floor(phase)`.

The pps result feeds the emit accumulator (`emitAccum += scale * ppsValue * 0.001 * frameTimeMs`),
i.e. this directly governs emit timing / particle count per frame. The
non-looping "effect is finished" flag (bit 3 / value 8, checked in
`zCParticleFX::CalcIsDead` @ `0x005af0d0`) and the looping wrap of the phase are
applied separately and already match OpenGothic's `prefferedTime` clamp.

## OpenGothic file:line

`/Users/admin/Downloads/opengothic/game/graphics/pfx/particlefx.cpp:292-315`
(`ParticleFx::fetchScaleKey`), used by `ParticleFx::ppsScale`
(`particlefx.cpp:186`) and `ParticleFx::shpScale` (`particlefx.cpp:181`), which
feed the emit count via `PfxBucket::ppsDiff` (`pfxbucket.cpp:22`) and the shape
scale via `PfxBucket::init` (`pfxbucket.cpp:278`).

## Divergence

For the **non-smooth** case OpenGothic samples the *nearest* keyframe:

```cpp
  if(alpha<0.5)
    return k0; else
    return k1;
```

whereas the original holds the *floor* keyframe (`key[floor(phase)]`) for the
entire key interval. As a result, OpenGothic advances to the next key half a
key-step early for every key. For a typical fade-out pps curve (e.g.
`pps_scale_keys = "1 0.5 0"`) this shifts the whole emit-rate-vs-time profile
forward by half a key interval, so particle emission ramps down (and an emitter's
visible output ceases) noticeably earlier than in the original. The same routine
is used for `shpScale`, so emitter-shape scaling is shifted identically.

The smooth branch already matches the original (linear interpolation toward the
next key), so only the non-smooth branch needs correcting.

## Proposed patch

```cpp
// OLD (particlefx.cpp:310-314)
  if(smooth)
    return k0+alpha*(k1-k0);
  if(alpha<0.5)
    return k0; else
    return k1;

// NEW
  if(smooth)
    return k0+alpha*(k1-k0);
  // NOTE: in original-game zCParticleFX::GetNumParticlesThisFrame @0x005b1a90 and
  // zCParticleFX::GetShapeScaleThisFrame @0x005b1920, a non-smooth scale key holds
  // key[floor(phase)] (step function) for the whole key interval and never rounds
  // to the nearest key, so emit-rate/shape transitions happen on integer key
  // boundaries, not half a key early.
  return k0;
```

`k0`, `k1`, `alpha`, `smooth`, `frameA`/`frameB` and `keys` are all grep-verified
local symbols of `ParticleFx::fetchScaleKey`. The change is local, build-safe and
leaves the smooth and key-count==0 paths untouched.
