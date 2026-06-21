# Issue #899 — Implement missing sound options (soundEnabled, soundUseReverb)

**Category:** sound/config · **Disposition:** split — `soundEnabled` already
works (verify/close); `soundUseReverb` DEFER (no reverb subsystem exists).

## Intended behavior (original)
- `SOUND/soundEnabled` — master toggle for SFX.
- `SOUND/soundUseReverb` — enables environmental reverb (original used EAX/A3D
  environment presets per zone).

## OpenGothic — current state
- `SOUND/soundEnabled` IS read and applied:
  - default set in `game/gothic.cpp:154`.
  - `Gothic::settingsSoundVolume()` (`game/gothic.cpp:859-864`) returns
    `soundEnabled ? soundVolume : 0.f`.
  - applied via `sndDev.setGlobalVolume(...)` in `Gothic::setupSettings()`
    (`game/gothic.cpp:913-914`) and re-applied on settings change.
  So `soundEnabled=0` already silences SFX. (Music has the parallel
  `musicEnabled` path in `game/gamemusic.cpp:388-411`.)
- `soundUseReverb` — NOT read anywhere. Grep for `reverb`/`EAX`/`environment`
  across `game/` returns nothing. `game/world/worldsound.cpp` has no reverb
  hooks, and `Tempest::SoundDevice`/`SoundEffect` are used without any
  environment/reverb API.

## Gap
- `soundEnabled`: effectively implemented; the issue may simply predate the
  volume-gating code. Recommend verifying and closing that half.
- `soundUseReverb`: requires (a) a reverb/environment capability in the
  Tempest sound backend (OpenAL EFX or equivalent), (b) per-`zCVobSound`/zone
  reverb preset selection in `game/world/worldsound.cpp`. None of this exists.

## Recommendation
- `soundEnabled`: no code change needed — confirm at runtime, close as done.
- `soundUseReverb`: DEFER. It is a new audio feature, not config plumbing, and
  depends on backend reverb support; out of scope for a surgical fix. If/when a
  reverb API lands in Tempest, read the flag in `Gothic::setupSettings()`
  alongside `settingsSoundVolume()` and pass it to the world-sound updater.
