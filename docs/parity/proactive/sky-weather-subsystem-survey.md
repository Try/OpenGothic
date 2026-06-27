# Sky / weather / rain subsystem parity survey

**Confidence:** DEFERRED (NO FINDING — no concrete constant/threshold/formula mismatch found)

## Scope investigated
Day/night sky-color interpolation times, rain start/stop scheduling, `Wld_SetTime`
effect on sky, rain-sound trigger, sky-layer cloud movement, moon/sun position by
game-time, fog-by-daytime, `Wld_IsRaining` / rain probability.

## Why there is no concrete numeric divergence to fix

1. **OpenGothic's sky is a clean custom physically-based renderer, not a numeric
   reimplementation of ZenGin's sky-state machine.**
   `game/graphics/sky/sky.cpp` derives sun position, day/night blend and lighting from
   suncalc-style constants that are explicitly OG-invented and documented as such in the
   source (e.g. `Sky::updateLight` rise=04:45 / meridian=13:09 / set=21:33;
   `Sky::isNight()` threshold `linearstep(-0.18f, 0.f, sun.dir().y)`;
   `Sky::cloudsOffset` periods 90000/270000 ms).
   The original (`zCSkyControler_Outdoor::Interpolate` @0x005e8c20) interpolates an
   8-entry array of `zCSkyState` keyframes whose times/colors come from the world ZEN and
   `SKY_OUTDOOR` INI settings. There is no shared numeric constant that OG "got wrong" —
   the two systems are architecturally different, so a value-for-value parity claim here
   would be a false positive.

2. **The entire weather / rain subsystem is unimplemented in OpenGothic — there is no
   constant to mismatch.**
   - `GameScript::wld_israining` (`game/game/gamescript.cpp:1940`) is a stub that logs
     "not implemented" and always returns `false`.
   - Grep over `game/` finds no rain scheduling, no `rainStart`/`rainStop`, no weather
     type, no `ProcessRainFX`/`SetEffectWeight` equivalent. The only "rain" hits are
     unrelated rendering/lightgroup code and the `zstartrain` marvin command name.
   - The original schedules rain by a dice roll over `rainStart`/`rainStop` game-time
     window and ramps the effect weight in `zCSkyControler_Outdoor::ProcessRainFX`
     @0x005eb0... (ramp-up over first 20% of the window via factor 5.0, full 1.0 across the
     middle 60%, ramp-down over the last 20%; rain sound `rain_01.wav`). None of this
     exists in OG, so this is a *missing-feature* gap, not a constant/threshold mismatch
     suitable for a surgical parity fix.

3. **The one parity-intended time constant in this area is already correct.**
   `World::setDayTime` (`game/world/world.cpp:391`) already carries a correct NOTE and
   matches the original `oCGame::SetTime` @0x006c4de0, which sets day via
   `param_2/0x18 + param_1` (i.e. `day + hour/24`) and keeps the current day otherwise.
   Verified against the decompile; no divergence.

## Disposition
No surgical, build-verifiable constant/threshold/formula fix is warranted. The real gap
(rain/weather scheduling + `Wld_IsRaining`) is a feature port, out of scope for a
high-confidence parity micro-fix. Empty beats false positives.
