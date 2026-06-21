# TA routine with `start == stop` is never selected as an active window

**Confidence:** Medium-High

## Original function + address (prose only)

Active daily-routine entry selection lives in `oCRtnManager::FindRoutine`
(Gothic2.exe @ `0x00775580`). It walks the NPC's routine list (stored as a
`zCListSort<oCRtnEntry>` sorted ascending by start time via
`oCRtnManager::Sort_Routine` @ `0x00774600`, key = start-hour at entry offset
`+0x00`, then start-minute at `+0x04`). For each entry belonging to the NPC it
calls `oCWorldTimer::IsTimeBetween` @ `0x00781190` with the four fields
`(start_h @+0x00, start_m @+0x04, stop_h @+0x08, stop_m @+0x0c)`.

`IsTimeBetween` computes `start = start_h*60 + start_m` and
`stop = stop_h*60 + stop_m`, then:

- If `start == stop`, it first does `stop = stop - oneMinute` (decrements the
  stop bound by one minute-unit). This turns an equal-bounds entry into a
  wrap-around window that spans almost the entire day.
- If `stop < start` (wrap window), it returns true when `start < cur` **OR**
  `cur < stop` (both strict).
- Otherwise (normal window) it returns true when `start < cur` **AND**
  `cur < stop` (both strict).

Net effect: an entry whose `start == stop` (e.g. the all-zeros `Rtn_Start_*`
that ships in real game data) is reported as the **active** routine for nearly
every minute of the day, because after the `stop -= 1min` adjustment it becomes
a wrap window that covers everything except the single excluded minute.

When no entry's window contains the current time, `FindRoutine` falls back to the
last non-matching entry walked (`*param_3`), i.e. the largest-start entry in the
sorted list.

## OpenGothic file:line

`game/world/objects/npc.cpp:3295` — `Npc::currentRoutine` (window test at lines
3302-3305).

```cpp
for(auto& i:routines) {
  if(assertWp && i.point==nullptr)
    continue;
  if(i.end<i.start && (time<i.end || i.start<=time))   // wrap window
    return i;
  if(i.start<=time && time<i.end)                       // normal window
    return i;
  }
```

## Divergence

OpenGothic has no handling for `i.start == i.end`:

- The wrap test `i.end < i.start` is false (they are equal, not less).
- The normal test `i.start <= time && time < i.end` reduces to
  `start <= time && time < start`, which is **always false**.

So an equal-bounds routine entry never matches an active window in OpenGothic.
It can only surface through the max-start fallback loop (lines 3312-3318), which
picks the entry with the largest start time among *all* entries.

In the original, such an entry is active almost the entire day. The two engines
therefore disagree whenever an NPC has a `start == stop` entry together with at
least one other entry that has a larger start time and whose own window does not
currently contain the time:

- Original: the equal-bounds entry wins (it is a near-all-day active window).
- OpenGothic: the larger-start entry wins via the fallback; the equal-bounds
  entry is ignored.

OpenGothic's own `Npc::endTime` (npc.cpp:3347) already special-cases
`r.start==r.end && r.end.toInt()==0` and cites "Rtn_Start_1081 in NTR is filled
with zeros", confirming equal-bounds entries occur in shipped data — but the
matching special-case is missing in `currentRoutine`, so `endTime` is only ever
reached for such an entry when it happens to also win the fallback.

## Proposed patch

Add the equal-window case as a matching active window, mirroring the original's
`stop -= 1min` semantics (an equal-bounds entry is active for the whole day
except its single boundary minute; in practice "active whenever no other window
matches" is what the original produces, so matching it unconditionally here is
the closest faithful behavior).

OLD (`game/world/objects/npc.cpp`, in `Npc::currentRoutine`):
```cpp
  for(auto& i:routines) {
    if(assertWp && i.point==nullptr)
      continue;
    if(i.end<i.start && (time<i.end || i.start<=time))
      return i;
    if(i.start<=time && time<i.end)
      return i;
    }
```

NEW:
```cpp
  for(auto& i:routines) {
    if(assertWp && i.point==nullptr)
      continue;
    if(i.end<i.start && (time<i.end || i.start<=time))
      return i;
    if(i.start<=time && time<i.end)
      return i;
    // NOTE: in original-game oCWorldTimer::IsTimeBetween @0x00781190, an entry with
    // start==stop is adjusted to stop-=1min, turning it into a wrap window that is
    // active for nearly the whole day; oCRtnManager::FindRoutine @0x00775580 then
    // reports it as the active routine. OpenGothic's start<time<end tests never match
    // an equal-bounds entry, so handle it explicitly here.
    if(i.start==i.end && i.start.toInt()<=time.toInt())
      return i;
    }
```

DEFERRED note on the boundary detail: matching `i.start==i.end` for the
`time>=start` half (and relying on the existing max-start fallback for the
`time<start` minute) reproduces the original's "active essentially all day"
behavior without re-deriving the exact one-minute exclusion. A fully literal
port would compute the single excluded minute `[stop-1min, start]`, but that
boundary minute is not observable in normal play and adding it risks a false
positive; the simpler guard above is the high-confidence change. Build-verify
after applying.
