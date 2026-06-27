# Camera close-in pullback uses bird's-eye snap instead of over-the-shoulder cam

> DEFERRED: a faithful fix requires reimplementing the original over-the-shoulder
> camera matrix (`zCMovementTracker::GetShoulderCamMat` + `zCAICamera::CalcAziElevRange`
> and its fixed azi/elev/offset constants). That is too large for a surgical,
> build-verifiable change and cannot be reduced to a single constant swap, so it is
> recorded here rather than patched. The threshold itself (80) is already correct.

**Confidence:** High that this is a genuine behavioral divergence; the fix is DEFERRED.

## Original function + address

`zCAICamera::AI_Normal` (Gothic2.exe @ 0x004a4370) computes the desired third-person
camera placement each frame. After projecting the best range/azimuth/elevation into a
world position (stored in the "first position" struct at `this+0x268`, position field
`+0x10`), it measures the distance between that desired camera position and the
movement-tracker's target/player position (`this+0x264`, field `+0x2c`):

- If that distance is **< 80.0** (constant `0x42a00000` = 80.0f), the engine discards
  the computed position and replaces it with `zCMovementTracker::GetShoulderCamMat`
  (@ 0x004ba380). That routine builds an **over-the-shoulder** matrix: it calls
  `zCAICamera::CalcAziElevRange` (@ 0x004bd7f0) with *fixed* shoulder azimuth/elevation
  globals (`DAT_008307e4`, `DAT_008307e8`) and a range of `velocityRange(this+0x280) +
  DAT_008307ec`, then offsets it behind/beside the hero at roughly eye level
  (`GetPoseOfHeading`). The camera stays low and behind the player, not overhead.

So in vanilla, when geometry forces the camera in close, it transitions into a
behind/over-the-shoulder framing whose pitch stays near the hero's eye level.

## OpenGothic file:line

`game/camera.cpp:830-843` (`Camera::tickThirdPerson`):

```
if(true && def.collision!=0) {
  auto rotation = calcLookAtAngles(inter.target + dir*range, inter.target, inter.rotOffset, state.spin);
  range = calcCameraColision(inter.target, dir, rotation, range);
  // NOTE: with range < 80, camera gradually moves up in vanilla
  if(range<80.f) {
    range      = 150; // also collision?!
    rotation.x = 80;
    rotation.y = state.spin.y;
    const auto rotOffsetMat = mkRotMatrix(rotation);
    dir = Vec3{0,0,1};
    rotOffsetMat.project(dir);
    }
  }
```

## Divergence

The `< 80` trigger matches the original (same 80.0 threshold, same meaning: the
camera has been pushed in close to the hero). The **response** diverges:

- Vanilla: switch to the over-the-shoulder matrix (`GetShoulderCamMat`), keeping the
  camera behind the hero near eye-level pitch, at a range derived from
  `velocityRange + DAT_008307ec`.
- OpenGothic: hard-snap `range = 150` and force `rotation.x = 80` (an ~80-degree
  downward, near top-down/bird's-eye pitch).

The `150` range and `80`-degree pitch are not present in the original logic (the
in-source comments "`// also collision?!`" and "`NOTE: with range < 80, camera
gradually moves up in vanilla`" acknowledge the values are guessed). The visible
result differs concretely: vanilla yields a behind-the-shoulder view, OpenGothic
yields an overhead look-down.

## Proposed patch

DEFERRED. A faithful fix is not a single-constant edit: it requires porting
`zCMovementTracker::GetShoulderCamMat` (@ 0x004ba380), which depends on
`zCAICamera::CalcAziElevRange` (@ 0x004bd7f0) and the fixed shoulder constants
`DAT_008307e4/e8/ec/f0/f4`, plus the target-extent velocity-range term (`this+0x280`,
set in `zCAICamera::SetTarget` @ 0x004a1120) that OpenGothic does not currently track.
Until those are reimplemented, the existing `range=150 / rotation.x=80` approximation
should stay, but is documented here as a known parity gap.

// NOTE: in original-game zCAICamera::AI_Normal @0x004a4370, when the desired
// camera-to-player distance < 80 the camera switches to
// zCMovementTracker::GetShoulderCamMat @0x004ba380 (over-the-shoulder), not to a
// fixed range=150 / elevation=80-degree bird's-eye view.
