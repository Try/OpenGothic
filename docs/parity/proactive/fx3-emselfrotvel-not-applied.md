# FX self-rotation (`emSelfRotVel`) is parsed and integrated but never applied to the emitter transform

**Confidence:** Medium (divergence certain on the OpenGothic side; exact original rotation
frame/axis convention not pinned down, so the fix is DEFERRED rather than surgical).

## Original function + address (prose only)

`oCVisualFX` parses `emSelfRotVel` (a degrees-per-second 3-vector) in its string/value
init pass (`oCVisualFX::ParseStrings` @0x0048be60 / `oCVisualFX::InitValues` @0x0048b820),
and per-key overrides arrive through `oCVisualFX::UpdateFXByEmitterKey` @0x0048ddc0.
The trajectory/orientation of the running FX vob is recomputed every frame in
`oCVisualFX::CalcTrajectory` @0x0048f620, which composes the emitter's frame each tick
(it repeatedly rebuilds the vob pose via `GetPoseOfHeading` / `GetTrafoModelNodeToWorld`).
A non-zero `emSelfRotVel` makes the original FX continuously spin about its own origin
over its lifetime (the field's whole purpose — "self rotation velocity"); FX authored
with it visibly rotate.

## OpenGothic file:line

`game/graphics/effect.cpp`

- Integration (works): `Effect::tick` lines 106-113 — accumulates
  `selfRotation += emSelfRotVel * dt/1000` and, when the velocity is non-zero, calls
  `syncAttachesSingle(pos)` each frame.
- Application (dead code): `Effect::syncAttachesSingle` lines 201-208:

```cpp
if(selfRotation!=Vec3() && false) {
  // FIXME
  Matrix4x4 m;
  m.rotateOX(selfRotation.x);
  m.rotateOY(selfRotation.y);
  m.rotateOZ(selfRotation.z);
  p.mul(m);
  }
```

Symbols verified to exist: `Effect::selfRotation` (`game/graphics/effect.h:83`),
`VisualFx::emSelfRotVel` (`game/graphics/visualfx.h:146`) and the per-key
`VisualFx::Key::emSelfRotVel` (`game/graphics/visualfx.h:95`), both wired into `tick`
and `syncAttachesSingle`.

## Divergence

The whole self-rotation feature is a no-op in OpenGothic. `tick()` faithfully integrates
the angle each frame, but the block that would fold that angle into the pose matrix `p`
(which is then pushed to `pfx`, `light`, `sfx` via `setObjMatrix`/`setPosition`) is gated
behind `&& false` and tagged `// FIXME`. Consequently any VisualFX whose
`emSelfRotVel`/per-key `emSelfRotVel` is non-zero renders without the continuous spin the
original applies; the emitter, its attached dynamic light and sound stay at a fixed
orientation instead of rotating about the FX origin. The accumulation in `tick()` is pure
overhead with no visible result.

## Proposed patch

DEFERRED.

Reason: The divergence is real and certain, but a high-confidence surgical fix is not.
Two unknowns block it:

1. **Rotation frame / convention.** The dead block post-multiplies `p.mul(m)` with an
   X-then-Y-then-Z Euler matrix. Whether the original spins about the emitter's *local*
   axes (post-multiply, in-place spin) or re-derives orientation from a heading vector
   (`GetPoseOfHeading` in `CalcTrajectory` @0x0048f620 suggests a heading-based rebuild,
   not a free 3-axis Euler compose) is not established here. Enabling the existing block
   verbatim risks the wrong axis order / wrong pivot (`p` already carries the bone or
   target translation, so a naive compose could rotate the translation as well as the
   basis). The OpenGothic authors' own `// FIXME` reflects the same uncertainty.

2. **Interaction with the attach path.** `syncAttachesSingle` overwrites `p` from the
   bone (`pose->bone(boneId)`) or target transform before reaching this block, so the
   spin must be layered onto an already-oriented matrix; getting the multiply side and
   pivot right (likely `T(t) * R * T(-t) * p` to spin about the FX origin rather than the
   world origin) needs a parity check against a known spinning FX in-game before it can be
   called build-verifiable.

Candidate one-line enablement (NOT recommended without the above parity verification):
remove the `&& false` guard at `game/graphics/effect.cpp:201` and confirm the pivot is the
FX origin. Until the original's rotation convention is confirmed against a concrete
`emSelfRotVel`-bearing effect, this stays DEFERRED to honor "empty beats false positives".
