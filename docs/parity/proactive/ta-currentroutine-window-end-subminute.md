# TA window match: active routine end bound is `stop-1min` inclusive in continuous time, not half-open `[start, stop)`

**Confidence:** Medium

## Original function + address (prose only)

Active daily-routine selection is `oCRtnManager::FindRoutine` (Gothic2.exe
`0x00775580`), which for each of the NPC's `oCRtnEntry` records calls the window
test `oCWorldTimer::IsTimeBetween` (`0x00781190`,
`P:\dev\g2addon\release\Gothic\_ulf\oWorld.cpp`) with the four fields
`(start_h @+0x00, start_m @+0x04, stop_h @+0x08, stop_m @+0x0c)`.

Decoding `IsTimeBetween` (the float idioms resolve to plain relational ops on a
time-of-day weight `start = start_h*Wh + start_m*Wm`, `stop = stop_h*Wh +
stop_m*Wm`, `now = *(float*)this`):

- It first does, **only when `start != stop`**, `stop = stop - Wm` — i.e. it
  subtracts exactly **one minute** from the stop bound. (`if ((NAN||NAN) ==
  (stop==start)) stop -= oneMinute;`, which for non-NaN inputs is `if (stop !=
  start) stop -= 1min`.)
- Non-wrap branch (`stop >= start`): returns `start <= now && now <= stop`.
- Wrap branch (`stop < start`): returns `start <= now || now <= stop`.

The comparison time `now` is **continuous**: `oCWorldTimer::Timer` (`0x00780d80`)
advances `*(float*)this` by a fractional per-frame increment (`SetTime`
`0x00780e40` builds the same hour/minute weight), so it is genuinely sub-minute,
not quantized to whole minutes. The one-minute decrement therefore makes the
effective active window `[start, stop-1min]` (upper bound inclusive of the
`stop-1min` instant) over continuous time — the open interval `(stop-1min, stop)`
is **not** covered by the entry.

## OpenGothic file:line

`/Users/admin/Downloads/opengothic/game/world/objects/npc.cpp:3512-3520`
(`Npc::currentRoutine`, the OG analogue of `FindRoutine`).

```cpp
auto time = owner.time().timeInDay();         // sub-minute (milliseconds)
for(auto& i:routines) {
  if(assertWp && i.point==nullptr)
    continue;
  if(i.end<i.start && (time<i.end || i.start<=time))  // wrap:  time<end (strict)
    return i;
  if(i.start<=time && time<i.end)                     // normal: time<end (strict)
    return i;
  }
```

`time` is `owner.time().timeInDay()` in milliseconds (unquantized; see
`game/game/gametime.h`), and `i.start`/`i.end` are whole-minute `gtime(h,m)`.

## Divergence

OpenGothic uses a strict `time < i.end` (half-open `[start, stop)`), while the
original treats the upper bound as `stop - 1 minute` inclusive (`[start,
stop-1min]`). Because OG feeds **sub-minute** `time` into the test, the two
disagree for any `time` in the final minute of a window, `[stop-60s, stop)`:

- Original `IsTimeBetween`: entry is **inactive** there (its window ends at
  `stop-1min`); `FindRoutine` then falls through to its no-match fallback (the
  largest-start entry — see `routine-gap-fallback.md`).
- OpenGothic `currentRoutine`: entry stays **active** for that last minute.

This is distinct from the four deferred TA items and from
`time-wld-istime-equal-bounds.md` (which targets `Wld_IsTime`/`wld_istime` and,
unlike `currentRoutine`, quantizes `now` to whole minutes) and from
`routine-gap-fallback.md` (which is about *which* fallback entry is chosen, not
the window width). It manifests when `currentRoutine` is sampled directly while
the NPC has no active AI state and must (re)start its routine — `tickRoutine`
(`npc.cpp:3227`), `currentTaPoint` (`3543`), `endTime` (`3546`) — e.g. on world
load / NPC placement / state end when the clock lands in a window's last game
minute: OG selects that window's entry/waypoint, the original selects the
largest-start fallback entry. (Tick-driven `oCRtnManager::CheckRoutines`
`0x007756c0` reacts on rising edges and is unaffected.)

Note: at distinct whole-minute `start`/`stop` the raw wrap test `i.end<i.start`
classifies identically to the original's decremented `stop-1min < start`, so only
the end bound needs adjusting; `start==stop` entries must keep flowing to the
fallback (they are the separate, deferred equal-window item) and so are skipped.

## Proposed patch

`game/world/objects/npc.cpp`, in `Npc::currentRoutine`:

OLD:
```cpp
    if(i.end<i.start && (time<i.end || i.start<=time))
      return i;
    if(i.start<=time && time<i.end)
      return i;
```

NEW:
```cpp
    if(i.start==i.end)
      continue; // equal-window entries resolve via fallback (separate deferred item)
    // NOTE: in original-game oCWorldTimer::IsTimeBetween @0x00781190 (driver of
    // oCRtnManager::FindRoutine @0x00775580) subtracts exactly one minute from the
    // stop bound when start!=stop and then compares with an inclusive <=, so the
    // active window is [start, stop-1min] over the *continuous* time-of-day. OG feeds
    // sub-minute timeInDay() here, so a strict time<end keeps the entry active one
    // game-minute too long (the original would already report no-match/fallback).
    const int64_t now  = time.toInt();
    const int64_t stop = i.end.toInt() - gtime(0,1).toInt(); // stop - 1 minute
    if(i.end<i.start) {            // midnight-wrapping window
      if(now<=stop || i.start<=time)
        return i;
      } else {                    // normal window
      if(i.start<=time && now<=stop)
        return i;
      }
```

`gtime::toInt()` and the `gtime(int,int)` constructor are public
(`game/game/gametime.h`); `gtime(0,1).toInt()` is the 60000 ms one-minute unit.
The added `start==end` skip preserves today's behavior for equal-window entries
(they already never matched the scan and fell to the fallback).
