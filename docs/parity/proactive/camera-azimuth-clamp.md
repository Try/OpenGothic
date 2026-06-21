# Camera azimuth clamp too wide in normal/combat modes

> DEFER: the exact set of camera modes taking the wide [-180,180] azimuth branch could not be confirmed from the decompilation; restricting normal-mode azimuth blindly risks a noticeable player-facing camera regression. Needs the mode-string membership validated (or runtime) before applying.

**Confidence:** Medium-High

## Original function

`zCAICamera::CheckKeys` (Gothic2.exe @ 0x004a45c0) reads the mouse/keyboard
look input each frame and clamps the camera's azimuth offset (the
"best_azimuth" delta, struct field 0x44) and elevation offset (field 0x38).

- Elevation (field 0x38) is clamped to **[-60.0, +85.0]** (constants
  `0xc2700000` = -60, `0x42a9ff7d` = 84.999) for every mode.
- Azimuth (field 0x44) is clamped in **two** ways depending on mode:
  - A small set of modes (the over-the-shoulder/interactive group: the
    `CAMMODMOB*` substring match plus two other mode-name matches) gets the
    wide range **[-180.0, +180.0]** (`0xc3340000` / `0x43340000`).
  - **All other modes** — including the primary gameplay mode whose name
    matches the *first* mode comparison in the function (`CamModNormal`) —
    fall through to the narrow clamp **[-80.0, +89.999]**
    (`0xc2a00000` = -80, `0x42b3ff7d` = 89.999).

  The wide branch condition is `(modeA || modeB || mobMatch)`; the Normal
  mode flag is computed separately and is *not* part of that condition, so
  Normal mode is provably governed by the narrow [-80, +90] azimuth clamp.

So in vanilla, while walking/running/fighting you can swing the camera only
about -80..+90 degrees in azimuth around the hero, not a full half-turn.

## OpenGothic

`game/camera.cpp:933` `Camera::clampRotation`:

```
float maxElev = +85;   // matches vanilla
float minElev = -60;   // matches vanilla
float maxAzim = +180;  // vanilla uses +90 in normal/combat modes
float minAzim = -180;  // vanilla uses -80 in normal/combat modes
```

The elevation bounds match the original exactly, but the azimuth bounds are
the *wide* [-180, +180] for every mode. `clampRotation` is called from both
`tickThirdPerson` (camera.cpp:811) and `tickFirstPerson` (camera.cpp:778),
with no mode discrimination.

## Divergence

In normal walk/run and the melee/ranged/magic combat modes the original
restricts free-look azimuth to roughly [-80, +90] degrees; OpenGothic allows
the camera to be spun a full [-180, +180]. This is engine-applied (the bounds
are hard-coded constants, not script `CamMod_*` values), so it is a concrete
behaviour difference, not a script artifact.

Note: the exact membership of the *wide-clamp* mode set (besides Normal being
excluded and MOB being included) could not be fully string-identified from the
decompiler, so the patch keeps the wide range for the interactive/mob family
and applies the narrow range to the regular modes only, mirroring the proven
structure.

## Proposed patch

```
// game/camera.cpp  (Camera::clampRotation, ~line 933)
OLD:
Vec3 Camera::clampRotation(Tempest::Vec3 spin) {
  //NOTE: min elevation is zero for nomal camera. assume that it's ignored by vanilla
  float       maxElev = +85;
  float       minElev = -60;
  float       maxAzim = +180;
  float       minAzim = -180;

NEW:
Vec3 Camera::clampRotation(Tempest::Vec3 spin) {
  //NOTE: min elevation is zero for nomal camera. assume that it's ignored by vanilla
  float       maxElev = +85;
  float       minElev = -60;
  // NOTE: in original-game (zCAICamera::CheckKeys @0x004a45c0) azimuth is
  // clamped to [-80,+90] in the regular gameplay/combat modes and only to
  // [-180,+180] for the mob/interactive camera family.
  bool        wideAzim = (camMod==Mobsi || camMod==Inventory || camMod==Dialog);
  float       maxAzim = wideAzim ? +180.f : +90.f;
  float       minAzim = wideAzim ? -180.f : -80.f;
```

(If exact-vanilla fidelity is desired, validate which two non-MOB modes take
the wide branch and adjust `wideAzim` accordingly; the Normal/Melee/Ranged/
Magic narrow clamp is the load-bearing change.)
