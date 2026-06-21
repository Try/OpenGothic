# weather: zCVobSoundDaytime window crossing midnight never plays the daytime sound

**Confidence:** High

## Original function + address

`zCVobSoundDaytime::DoSoundUpdate` (Gothic2.exe `0x0063ef50`), helper
`zCVobSoundDaytime::CalcTimeFrac` (`0x0063eec0`).

A `zCVobSoundDaytime` vob has two sounds (a primary `sndName` and a secondary
`sndName2`) and a time-of-day window `[startTime, endTime]` expressed in hours.
Each update the original computes the current time-of-day in hours from the
active outdoor sky controler (roughly `skyTime*24 + 12`, wrapped into `[0,24)`),
then decides whether the current time falls **inside** the window. Crucially the
inside-window test explicitly handles a window that crosses midnight: in both
`DoSoundUpdate` and `CalcTimeFrac` the original does

- if `endTime < startTime` then add 24 to `endTime` (and, when the current time
  is below `startTime`, add 24 to the current time too),
- then `frac = (time - startTime) / (endTime - startTime)` and the window is
  considered active when `0 <= frac < 1`.

So a window such as `20:00 .. 06:00` (a night ambient, e.g. crickets/owls) is a
valid wrapped window that is active from 20:00 through 06:00. When active the
primary sound plays (with a 0.1/0.9 linear fade envelope at the window edges);
when inactive the secondary sound plays.

## OpenGothic file:line

`game/world/worldsound.cpp:186-191` (selection of `eff0` vs `eff1` in
`WorldSound::tick`).

## Divergence

OpenGothic decides which sound to play with a plain, non-wrapping interval test:

```cpp
if(i.sndStart<= time && time<i.sndEnd) {
  snd = i.eff0;
  } else {
  snd = i.eff1;
  }
```

`sndStart`/`sndEnd` are filled directly from the vob's `start_time`/`end_time`
(worldsound.cpp:107-108) with no normalization. When a daytime-sound vob defines
a window that crosses midnight (`start_time > end_time`, e.g. `20.0 .. 6.0`), the
condition `sndStart <= time && time < sndEnd` is **never** satisfied for any
time-of-day, because there is no instant that is simultaneously `>= 20:00` and
`< 06:00`. As a result OpenGothic plays the secondary sound `eff1` for the entire
day and never plays the intended windowed primary sound `eff0`. The original
plays `eff0` throughout the wrapped 20:00..06:00 window. (Non-wrapping windows,
`start_time <= end_time`, behave identically in both.)

## Proposed patch

`game/world/worldsound.cpp`, in `WorldSound::tick`:

OLD:
```cpp
    const SoundFx* snd = nullptr;
    if(i.sndStart<= time && time<i.sndEnd) {
      snd = i.eff0;
      } else {
      snd = i.eff1;
      }
```

NEW:
```cpp
    // NOTE: in original-game zCVobSoundDaytime::DoSoundUpdate (Gothic2.exe 0x0063ef50)
    // the daytime window is treated modulo 24h: a window whose start is later than
    // its end (e.g. 20:00..06:00) wraps across midnight and is active across midnight.
    const bool inWindow = (i.sndStart<=i.sndEnd)
                            ? (i.sndStart<=time && time<i.sndEnd)
                            : (i.sndStart<=time || time<i.sndEnd);
    const SoundFx* snd = inWindow ? i.eff0 : i.eff1;
```

Verified symbols exist: `WorldSound::Effect::sndStart`, `sndEnd`, `eff0`, `eff1`
(worldsound.cpp:23-37); `time` is the local `gtime(0,hour,minute)` computed at
worldsound.cpp:183-184. The edge-fade volume envelope (0.1/0.9) in the original
is intentionally not reproduced here — that is a volume-shaping nicety, separate
from the start/stop logic; this patch fixes only the wrapped-window selection.
