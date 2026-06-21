# Issue #920 — Time domain & stacking of "time.slw" (swampweed)

## Issue
Smoking several swampweed in a row compounds the world slow-time effect to a
near-frozen, unrecoverable speed. Expected (original Gothic II): the slow-time
effect from swampweed is a single global time scaler that does **not** multiply
with itself when re-applied, and it wears off; re-using the item restarts the
same effect rather than stacking another factor on top.

## Subsystem & OG files
- `game/game/globaleffects.cpp` — `GlobalEffects::scaleTime`,
  `GlobalEffects::addSlowTime`, `GlobalEffects::tick`, `stopEffect`.
- `game/game/globaleffects.h` — `SlowTime`, `timeEff` vector.
- `game/graphics/effect.cpp:52-62` — `Effect::setupPfx` routes `time.slw` to
  `World::addGlobalEffect` with `emFXLifeSpan` as duration.
- `game/game/gamesession.cpp:306` / `game/world/world.cpp:371` — per-frame
  `scaleTime(dt)` driving world time.

## Original behavior (Gothic2.exe — functions + addresses, prose)
The slow-time VisualFX is the `.SLW` global FX (decl string @ 0x00897064),
managed through `oCVisualFX` (ctor 0x0048a010, `Play` 0x0048a050,
`SetDuration` 0x0048a060) and torn down via `oCVisualFX::RemoveInstancesOfClass`
(0x00489a10). In the original a global time-scale FX defines a *single* world
time multiplier with a finite duration; re-triggering it on an already-active
instance restarts/replaces that one multiplier rather than introducing a second
independent factor. The net world time-scale is therefore bounded by one FX's
multiplier, and it expires after its duration, returning the world to 1.0x.
(`oCVisualFX::SetDuration` writes the single duration field at this+0x2c8.)

## OpenGothic current behavior (file:line)
- `globaleffects.cpp:159-178` `addSlowTime` **always appends** a new `SlowTime`
  to `timeEff` (`timeEff.emplace_back(...)`). Nothing replaces an existing one.
- `globaleffects.cpp:40-47` `scaleTime` then **multiplies dt by every** active
  `SlowTime`: `dt = dt*i.mul + rem; dt /= i.div;` inside a loop over `timeEff`.
  Four 0.5x stacks ⇒ 0.5^4 ≈ 0.0625x world speed.
- `globaleffects.cpp:124-132` `startEffect`: when `len==0` (i.e. the decl's
  `emFXLifeSpan` is 0, as for `time.slw`) it sets `timeUntil = uint64_t(-1)`, so
  `tick` (line 28-38) never erases it — the effect is effectively permanent.
- `stopEffect` (line 135-145) already calls `timeEff.clear()` for `time.slw`,
  confirming the engine elsewhere assumes a *single* logical slow-time.

## Divergence
Two compounding defects vs. the original: (1) concurrent multiplicative
stacking in `scaleTime`, and (2) infinite lifetime when `emFXLifeSpan==0`.
Together they make repeated swampweed use drive world speed toward zero with no
recovery. The original keeps one bounded, expiring time scaler.

## Proposed patch
Make `time.slw` re-application replace the existing slow-time instead of
stacking. This is surgical, matches `stopEffect`'s single-instance assumption,
and fixes the unrecoverable case without touching the duration/lifespan path.

File: `game/game/globaleffects.cpp`

OLD:
```cpp
GlobalFx GlobalEffects::addSlowTime(const std::string* argv, size_t argc) {
  double val[2] = {1,1};
  for(size_t i=0; i<argc && i<2; ++i) {
    try {
      val[i] = std::stof(argv[i]);
      }
    catch(...) {
      Log::e("invalid time.slw parameter [",i,"]: \"", argv[i], "\"");
      }
    }
  uint64_t v[2] = {};
  for(int i=0; i<2; ++i)
    v[i] = uint64_t(val[i]*1000);
  // todo: separated time-scale
  SlowTime s;
  s.mul = v[0];
  s.div = 1000;
  timeEff.emplace_back(std::make_shared<SlowTime>(s));
  return GlobalFx(timeEff.back());
  }
```

NEW:
```cpp
GlobalFx GlobalEffects::addSlowTime(const std::string* argv, size_t argc) {
  double val[2] = {1,1};
  for(size_t i=0; i<argc && i<2; ++i) {
    try {
      val[i] = std::stof(argv[i]);
      }
    catch(...) {
      Log::e("invalid time.slw parameter [",i,"]: \"", argv[i], "\"");
      }
    }
  uint64_t v[2] = {};
  for(int i=0; i<2; ++i)
    v[i] = uint64_t(val[i]*1000);
  // todo: separated time-scale
  SlowTime s;
  s.mul = v[0];
  s.div = 1000;
  // NOTE: in original-game the .SLW slow-time is a single global world time
  // scaler (oCVisualFX, RemoveInstancesOfClass @0x00489a10) — re-using
  // swampweed restarts that one effect, it does NOT add a second independent
  // factor. Replace any active slow-time instead of stacking, otherwise
  // scaleTime() multiplies all of them together and world speed collapses
  // toward zero (issue #920).
  timeEff.clear();
  timeEff.emplace_back(std::make_shared<SlowTime>(s));
  return GlobalFx(timeEff.back());
  }
```

## Notes / residual
This fix removes the unrecoverable stacking (the reported showstopper). The
secondary "never expires" behavior (timeUntil=-1 when `emFXLifeSpan==0`) is left
as-is because the correct duration source is the VisualFx decl / item script
(`emFXLifeSpan` or the swampweed `time.slw` keyframe length) and changing the
lifespan default risks regressing other global FX (`morph.fov`,
`earthquake.eqk`, `screenblend.scx`) that share the same `startEffect` path.
With stacking removed, a single 0.5x scaler is fully playable, and existing
`stopEffect("time.slw")` paths still cancel it. If runtime testing later shows
swampweed should auto-expire, that is a follow-up scoped to the decl/script
lifespan, not the stacking math.
