# Routine `endTime` misclassifies nonzero `start==end` slots as "not active", restarting the state every AI tick

**Confidence:** High

## Original function + address (prose only)

A daily-routine slot whose start time equals its stop time (e.g. `TA_MIN(self, 8,0, 8,0, ZS_..., wp)`) is a fully legitimate, very common "all-day" routine in Gothic — most of the convenience wrappers (`TA_Stand_Guarding(8,0,8,0)`, `TA_Smalltalk(8,0,8,0)`, etc.) are routinely invoked with identical start and stop times to mean "do this all day".

In the original engine there is no per-slot "end time" at all: `oCRtnManager::CheckRoutines` (@0x007756c0) polls `oCWorldTimer::IsTimeBetween` (@0x00781190) every frame, and the active slot is (re)selected by `oCRtnManager::FindRoutine` (@0x00775580). `IsTimeBetween` builds the start/stop fractions and, because it only subtracts one minute from the end when `start != end`, a `start==end` slot evaluates as active at exactly the start minute; for the rest of the day `FindRoutine` returns it as the fallback (its single/largest-start entry). The net effect: a `start==end` routine runs continuously, it is never an "error" and never terminated each frame.

## OpenGothic file:line

`/Users/admin/Downloads/opengothic/game/world/objects/npc.cpp:3431` — `Npc::endTime(const Routine&)`, specifically the tail at lines 3446-3451.

## Divergence

`endTime` is the only mechanism OpenGothic uses to bound a routine state: `tickRoutine` (npc.cpp:3127) stores it in `aiState.eTime`, and once `aiState.eTime<=owner.time()` the state is forced to `LOOP_END` and cleared (npc.cpp:3172-3183), after which the next slot is re-selected.

`endTime`'s three branches cover only `end<start` (wrap), `start<end` (normal), and `start==end && end.toInt()==0` (the all-zeros NTR `Rtn_Start_1081` case). A slot with `start==end` and a **nonzero** time matches none of them and falls through to:

```
  // error - routine is not active now
  return wtime;
```

i.e. it returns the current time. So `aiState.eTime` is set to "now", `tickRoutine` immediately treats the routine state as expired (`eTime<=time`), runs the ZS `_End` function, clears the state, and on the very next tick `currentRoutine()` returns the same `start==end` slot again and restarts it — re-running the ZS `_Loop`/init. The NPC's all-day routine state is torn down and re-initialised every AI tick instead of running continuously, diverging from the original where the slot simply stays active. The existing all-zeros special case shows the authors already know `start==end` should mean "all day"; it was just never generalised to nonzero times.

## Proposed patch

Generalise the all-zeros special case to every `start==end` slot (which makes the three conditions `<`/`>`/`==` exhaustive, so the misleading "error" fall-through is no longer reached for valid routines). Grep-verified symbols: `gtime::day()`, `gtime::hour()`, `gtime::minute()`, `gtime(int64_t,int32_t,int32_t)` all exist in `game/game/gametime.h`; `Routine::start`/`Routine::end` exist in `game/world/objects/npc.h:427-428`.

OLD (`game/world/objects/npc.cpp:3446-3451`):
```cpp
  if(r.start==r.end && r.end.toInt()==0) {
    // for example Rtn_Start_1081 in NTR is filled with zeros
    return gtime(wtime.day()+1,r.end.hour(),r.end.minute());
    }
  // error - routine is not active now
  return wtime;
```

NEW:
```cpp
  if(r.start==r.end) {
    // NOTE: in original-game a routine slot with start==end (e.g. the common all-day wrappers
    // TA_Stand_Guarding(8,0,8,0) / TA_Smalltalk(8,0,8,0), and the all-zeros Rtn_Start_1081 in NTR)
    // is NOT an error: oCWorldTimer::IsTimeBetween @0x00781190 reports it active at the start
    // minute and oCRtnManager::FindRoutine @0x00775580 keeps it as the active/fallback routine for
    // the rest of the day, so the state must run continuously. Treat it as a full-day window that
    // ends at the same time next day instead of returning `now` (which forced an immediate
    // LOOP_END and restarted the routine state every AI tick).
    return gtime(wtime.day()+1,r.end.hour(),r.end.minute());
    }
  // error - routine is not active now
  return wtime;
```

The `r.end.toInt()==0` all-zeros instance is preserved unchanged (`end.hour()==end.minute()==0` → `gtime(day+1,0,0)`), so no regression for `Rtn_Start_1081`.
