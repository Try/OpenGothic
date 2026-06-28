# Clock-advance rate: game-time runs 0.69% too fast (14.5 vs 14.4 game-ms per real ms)

**Confidence:** High

## Original fn + address

The world clock in `Gothic2.exe` is driven once per rendered frame by
`oCWorldTimer::Timer` @ `0x00780d80`, called from `oCGame::Render` @ `0x006c87a0`
(the non-loading branch, guarded by the "clock paused" flag `DAT_00ab0888`).

The timer keeps a float `timeOfDay` accumulator (member offset `+0` of `oCWorldTimer`)
plus an `int day` counter (offset `+4`). Each frame `Timer` does:

```
timeOfDay += DAT_0099b3d8;          // ztimer.frameTimeFloat  (real frame time, in ms)
if (timeOfDay > fullDay) {          // fullDay = _DAT_00ab371c = 6,000,000.0
    timeOfDay -= fullDay;
    ++day;
    ...RefreshNpcs / CheckRoutines  // day rollover
}
```

The accumulator's unit-to-clock mapping is fixed by `oCWorldTimer::SetTime` @ `0x00780e40`
and `oCWorldTimer::GetSkyTime` @ `0x00781240`:

- per game-hour constant `_DAT_0083e168 = 250000.0` (verified by reading the `.rdata`
  float at VA `0x0083e168`),
- full day `_DAT_00ab371c = 24 * 250000 = 6,000,000.0` (confirmed: `GetSkyTime` divides
  `timeOfDay` by `_DAT_0083e168 * 24.0`).

The per-frame increment `DAT_0099b3d8` is field `+4` of the global `ztimer` object
(`zCTimer` block based at `0x0099b3d4`); `zCTimer::ResetTimer`/`SetFrameTime`
@ `0x005f96b0`/`0x005f9800` store the real frame delta there in **milliseconds**
(clamped to <=100). So one millisecond of real time adds exactly **1.0** timer-unit.

### Derivation of the original ratio

- 1 game-hour = 250000 timer-units, and 1 timer-unit accumulates per 1 real millisecond.
- 1 game-hour = 3,600,000 game-milliseconds.
- Therefore the clock advances `3,600,000 / 250,000 = 14.4` game-ms per real ms.
- A full game day (`6,000,000` units) takes `6,000,000` real ms = **100 real minutes**,
  frame-rate independent (the increment is summed frame time, not a per-frame constant).

## OG file:line

`/Users/admin/Downloads/opengothic/game/game/gamesession.cpp:20-22`

```cpp
// rate 14.5 to 1
const uint64_t GameSession::multTime=14500;
const uint64_t GameSession::divTime =1000;
```

Used in `GameSession::tick` @ `gamesession.cpp:323-325`:
`add = dt*multTime + wrldTimePart; wrldTime.addMilis(add/divTime);`
With `divTime=1000`, this adds `dt * 14.5` game-ms per real ms `dt`
(`wrldTime` is a `gtime` measured in game-ms, `dayMilis = 86,400,000`).
`World::scaleTime` only applies the global-FX slow/freeze and does **not** touch the
base clock, so the steady-state base rate is purely `multTime/divTime = 14.5`.

## Divergence

OpenGothic advances the world clock at **14.5** game-ms per real ms; the original
advances it at **14.4**. OpenGothic's clock therefore runs ~0.694% fast: a full
in-game day completes in `86,400,000 / 14.5 = 5,958,620` real ms (~99.31 min) instead
of the original `6,000,000` real ms (100.0 min) — about 41 real seconds early per day,
accumulating across long sessions and shifting every TA / day-night / sky transition.

## Proposed patch

```cpp
// OLD
// rate 14.5 to 1
const uint64_t GameSession::multTime=14500;
const uint64_t GameSession::divTime =1000;

// NEW
// NOTE: in original-game oCWorldTimer::Timer @0x00780d80 adds the real frame time (ms) to a
// timer accumulator where 1 game-hour == 250000 units (oCWorldTimer::SetTime @0x00780e40,
// _DAT_0083e168=250000.0); 1 game-hour == 3,600,000 game-ms => clock rate = 3.6e6/2.5e5 = 14.4
// game-ms per real ms (full day = 6,000,000 real ms = 100 min). Use 14400, not 14500.
// rate 14.4 to 1
const uint64_t GameSession::multTime=14400;
const uint64_t GameSession::divTime =1000;
```
