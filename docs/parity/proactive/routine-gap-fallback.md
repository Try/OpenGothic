# Routine selection: no-active-routine fallback picks wrong entry in coverage gaps

**Confidence:** Medium

## Original function

`oCRtnManager::FindRoutine` @ `0x00775580` (helper `oCWorldTimer::IsTimeBetween`
@ `0x00781190`; entries kept in a list sorted by start time via
`oCRtnManager::Sort_Routine` @ `0x00774600`, which orders by start-hour then
start-minute ascending).

FindRoutine walks the NPC's routine entries in start-sorted order. For each it
asks IsTimeBetween whether the current time-of-day lies in `[start, end)`
(start inclusive, end exclusive at minute resolution; with midnight wraparound
when end < start). The **first** entry whose window contains the current time is
returned as the active routine.

When **no** entry's window contains the current time (a gap in the schedule),
FindRoutine falls back to the last non-active entry it visited while walking the
sorted list. Because the list is sorted by start time and every entry is
non-active in a gap, that last-visited entry is simply the entry with the
**largest start time** (the final list element). The decision is purely
positional: "the routine that starts latest in the day."

## OpenGothic

`game/world/objects/npc.cpp:3261` `Npc::currentRoutine`. The active-window scan
(lines 3265-3272) matches the original: it returns the first routine with
`start<=t<end`, with correct wraparound at 3268. That part is fine.

The fallback (lines 3274-3293) does **not** match. OpenGothic computes, for each
entry, `delta = (t - end) mod 24h` and returns the entry minimizing `delta`,
i.e. the routine whose **end** time most recently passed before the current
time. The original instead returns the routine with the **largest start time**.

## Divergence

These pick different entries inside a coverage gap. Example schedule (sorted by
start): A 08:00-10:00, B 12:00-14:00, C 20:00-22:00. At 11:00 (gap between A and
B), nothing is active:

- Original FindRoutine -> **C** (largest start / last list entry).
- OpenGothic -> **A** (its end 10:00 is the most-recently-passed end).

Different fallback routine => different waypoint, different routine state and
`isInRoutine` result while the NPC is in the gap. Only triggers for schedules
that leave a time-of-day uncovered; most stock schedules cover 24h, hence
Medium rather than High.

## Proposed patch

```cpp
// FILE: game/world/objects/npc.cpp  (Npc::currentRoutine, fallback block)
// OLD:
  const auto     day   = gtime(24,0).toInt();
  const Routine* rtn   = nullptr;
  int64_t        delta = std::numeric_limits<int64_t>::max();
  for(auto& i:routines) {
    if(assertWp && i.point==nullptr)
      continue;
    int64_t d = time.toInt() - i.end.toInt();
    if(d<0)
      d += day;
    // take the last one if multiple with same end time exist
    if(d<=delta) {
      rtn   = &i;
      delta = d;
      }
    }

// NEW:
  // NOTE: in original-game oCRtnManager::FindRoutine, when no routine window
  // contains the current time, the fallback is the last non-active entry in the
  // start-sorted list, i.e. the routine with the largest start time - not the
  // routine whose end most recently passed.
  const Routine* rtn = nullptr;
  for(auto& i:routines) {
    if(assertWp && i.point==nullptr)
      continue;
    rtn = &i; // routines are start-sorted; keep the last (largest start)
    }
```
