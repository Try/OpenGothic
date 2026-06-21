# Day/Night music threshold diverges from original (6:30-18:30 vs 4:00-21:00)

**Confidence:** High

## Original function + address

`oCZoneMusic::IsDaytime` (0x00642400) is the function the music-zone system uses to
choose between the DAY and NGT theme variants. It does nothing but fetch the world
timer (`oCGame::GetWorldTimer`) and return `oCWorldTimer::IsDay`.

`oCWorldTimer::IsDay` (0x00781280) and its mirror `IsNight` (0x00781300) compare the
current intra-day clock fraction against two fixed boundaries built from the engine's
hour-scale (1/24 of a day) and minute-scale (1/1440 of a day) constants:
- lower boundary = 6 hours + 30 minutes -> **06:30**
- upper boundary = 18 hours + 30 minutes -> **18:30**

`IsDay` returns true only when the clock is within [06:30, 18:30); outside that window it
returns false (night). `GetNewTheme` (0x00641ba0) then appends `_DAY` when `IsDaytime`
is set and `_NGT` otherwise. So in the original, music switches to night themes at
18:30 and back to day themes at 06:30.

## OpenGothic location

`game/world/worldsound.cpp:241`

```cpp
bool  isDay = (gtime(4,0)<=time && time<=gtime(21,0));
```

The DAY/NGT music tag is then derived from this `isDay` flag (lines 252, 261).

## Divergence

OpenGothic treats 04:00-21:00 as "day" for music selection. The original treats
06:30-18:30 as day. Concrete, audible gameplay difference:
- From 04:00 to 06:30 OpenGothic plays DAY music; original still plays NGT.
- From 18:30 to 21:00 OpenGothic still plays DAY music; original already switched to NGT.

Roughly 5 hours per day get the wrong music variant relative to the original game.
The boundaries are also inclusive/exclusive differently, but the 2.5h offset at each
edge is the substantive issue.

## Proposed patch

File: `game/world/worldsound.cpp`

OLD:
```cpp
  gtime time  = owner.time().timeInDay();
  bool  isDay = (gtime(4,0)<=time && time<=gtime(21,0));
```

NEW:
```cpp
  gtime time  = owner.time().timeInDay();
  // NOTE: in original-game oCZoneMusic::IsDaytime defers to oCWorldTimer::IsDay,
  // which uses fixed boundaries of 06:30 (inclusive) and 18:30 (exclusive).
  bool  isDay = (gtime(6,30)<=time && time<gtime(18,30));
```
