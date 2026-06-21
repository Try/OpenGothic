# Wld_SetTime / wait-menu spuriously advances to the next day

**Confidence:** High

## Original function + address (prose)

`Wld_SetTime` is registered by `oCGame::DefineExternals_Ulfi` (0x6d4780) to the
C handler at **0x6dde40** (`P:\dev\g2addon\release\Gothic\_ulf\oGameExternal.cpp`).
That handler:

1. Calls `oCGame::GetTime` (vtable +0x48, impl 0x6c4e70) to read the **current**
   day, hour and minute.
2. Pops the two Daedalus parameters (hour, minute).
3. Calls `oCGame::SetTime(currentDay, hour, min)` (vtable +0x44, impl 0x6c4de0).

`oCGame::SetTime(day,hour,min)` (0x6c4de0) does:
- `oCWorldTimer::SetDay(hour/24 + day)`  (SetDay at 0x780de0 just stores the int)
- `oCWorldTimer::SetTime(hour, min)` (0x780e40), which stores
  `(hour % 24)` and `(min % 60)` as the clock.

So with the script range `hour in [0,23]`, `hour/24 == 0` and the resulting day is
**always exactly the current day**. The original never compares the requested time
against the current time: setting an *earlier* time of day simply moves the clock
**backward within the same day**. The day counter (`Wld_GetDay`) only increases when
`hour >= 24` is passed explicitly.

## OpenGothic file:line

`game/world/world.cpp:391-403` (`World::setDayTime`), reached by both
`GameScript::wld_settime` (`game/game/gamescript.cpp:1630`) and the marvin
`settime` console command (`game/marvin.cpp:586`).

## Divergence

OpenGothic adds a branch that the original does not have: when the requested
time-of-day is *earlier* than the current time-of-day (`dayTime > next`) it bumps
to `day+1`. Concrete case: it is 22:00 on day 3 and a script (or the Gothic 2
wait-menu, which calls `Wld_SetTime`) requests 08:00.

- Original: clock becomes **08:00, day 3** (time runs backward).
- OpenGothic: clock becomes **08:00, day 4** (an extra day elapses).

This shifts the global day counter by one every time `Wld_SetTime` is used to set a
time earlier in the same day. It is gameplay-visible through `Wld_GetDay`-driven
quest logic, day-gated dialogue/spawns, and any "N days passed" bookkeeping.

## Proposed patch

```cpp
// game/world/world.cpp
// OLD
void World::setDayTime(int32_t h, int32_t min) {
  gtime now     = game.time();
  auto  day     = now.day();
  gtime dayTime = now.timeInDay();
  gtime next    = gtime(h,min);

  if(dayTime<=next){
    game.setTime(gtime(day,h,min));
    } else {
    game.setTime(gtime(day+1,h,min));
    }
  wobj.resetPositionToTA();
  }

// NEW
void World::setDayTime(int32_t h, int32_t min) {
  // NOTE: in original-game, oCGame::SetTime keeps the CURRENT day and only
  // sets the clock to (h%24, min%60); the day advances solely via hour/24.
  // Requesting an earlier time-of-day moves the clock backward within the same
  // day, it does NOT roll over to the next day.
  gtime now = game.time();
  auto  day = now.day() + h/24;

  game.setTime(gtime(day, h%24, min%60));
  wobj.resetPositionToTA();
  }
```
