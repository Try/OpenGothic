# PFX parity: LINE emitter shape sampled as one-sided 3D diagonal instead of centered Y-axis segment

**Confidence:** Medium-high (concrete, decompiler-verified shape-sampling divergence; LINE emitters are
comparatively rare, hence not "high").

## Original function + address

`zCParticleEmitter::GetPosition` @ `0x005b4880` computes a freshly emitted particle's local spawn
position by `switch`-ing on the shape type (field `+0x28c`: 0=POINT, 1=LINE, 2=BOX, 3=CIRCLE,
4=SPHERE, 5=MESH). For the **LINE** case (case 1) the original writes exactly:

- `out.y = zRandF2() * shpDim.x`  (field `+0x294`, the first component of `String2Vec3(shpDim)`)
- `out.x = 0`
- `out.z = 0`

`zRandF2` @ `0x005b54..` returns `(rand()-16383.5)/16384`, i.e. a value in **[-1, 1]** (verified: the
BOX case at the same address uses the identical `zRandF2` expression per axis, and OpenGothic's BOX
matches it with `randf()*2-1`). So the original LINE is a segment that lives **only on the emitter's
local Y axis**, **centered on the origin** (range `[-shpDim.x, +shpDim.x]`).

(The deterministic branch on field `+0x2b4` is the UNIFORM/WALK `shpDistribType` ring-buffer, a
separate, already-TODO'd gap — not part of this finding.)

## OG file:line

`game/graphics/pfx/pfxbucket.cpp:220-224` (`PfxBucket::init`, `EmitterType::Line` case):

```cpp
case ParticleFx::EmitterType::Line:{
  float at = randf();
  p.pos = Vec3(at,at,at);
  break;
  }
```

followed by the generic per-axis scale at lines 276-282
(`p.pos.x*=dim.x; p.pos.y*=dim.y; p.pos.z*=dim.z;`).

## Divergence

OpenGothic samples `at = randf()` in **[0, 1)** and assigns it to **all three** axes
(`Vec3(at,at,at)`). After the generic `*= shpDim`, and because `Parser::loadVec3` broadcasts a scalar
shpDim string (e.g. `"30"`) to `(30,30,30)`, the LINE becomes a **3D diagonal** running from the
emitter origin `(0,0,0)` to `(shpDim, shpDim, shpDim)` — one-sided and ~`sqrt(3)` too long.

The original is a **1D segment on the local Y axis only**, **centered** on the origin
(`[-shpDim.x, +shpDim.x]`). Both the axis spread (diagonal vs. Y-only) and the centering
(`[0,1]` vs. `[-1,1]`) differ, so LINE-emitter particles spawn in a visibly wrong region.

## Proposed patch

```cpp
// OLD
case ParticleFx::EmitterType::Line:{
  float at = randf();
  p.pos = Vec3(at,at,at);
  break;
  }

// NEW
case ParticleFx::EmitterType::Line:{
  // NOTE: in original-game zCParticleEmitter::GetPosition @0x005b4880 a LINE emitter spawns the
  // particle on the local Y axis only, at offset zRandF2()*shpDim.x with zRandF2() in [-1,1]
  // (i.e. centered on the emitter origin). OpenGothic produced a one-sided 3D diagonal
  // Vec3(at,at,at), at in [0,1], spreading the segment over all three axes and starting at origin.
  float at = 2.f*randf()-1.f;
  p.pos = Vec3(0,at,0);
  break;
  }
```

The subsequent generic `p.pos.y *= dim.y` (lines 276-282) then yields `(0, at*shpDim.y, 0)`; since a
scalar shpDim broadcasts to `dim.y == shpDim.x`, this reproduces the original's `out.y = at*shpDim.x`
exactly for the scalar shpDim strings that LINE emitters use in practice.
