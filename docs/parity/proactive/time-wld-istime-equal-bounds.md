# Wld_IsTime returns false for zero-width window (begin == end), original returns true at that exact minute

**Confidence:** High

## Original function + address

`oCWorldTimer::IsTimeBetween(begin_h, begin_m, end_h, end_m)` at `0x00781190`
(`P:\dev\g2addon\release\Gothic\_ulf\oWorld.cpp`). This is the routine that the
script external `Wld_IsTime` is wired to, and it is also the function used by
`oCRtnManager::FindRoutine` (`0x00775580`) to decide which daily-routine entry is
active for the current time of day.

Decoded behavior (the float-comparison idioms in the decompile resolve to plain
relational ops on the normalized day fractions; `begin = h0/24 + m0/1440`,
`end = h1/24 + m1/1440`, `now` is the timer's current day fraction):

- It first computes `begin` and `end` as day fractions.
- It then conditionally subtracts exactly one minute from `end`, but **only when
  `begin != end`**. (In the decompile: `if ((NAN||NAN) == (end==begin)) end -= minuteWeight;`
  which for non-NaN inputs is `if (end != begin) end -= 1min;`.)
- Non-wrap branch (`end >= begin`): returns `begin <= now && now <= end`.
- Wrap branch (`end < begin`): returns `begin <= now || now <= end`.

Because the one-minute decrement is skipped when `begin == end`, the equal case
collapses to the non-wrap branch with `end == begin`, i.e. it returns
`begin <= now && now <= begin`, which is **`now == begin`**. In other words, a
zero-width window `[t, t]` is considered active for exactly the one minute `t`.
For every non-equal window the decrement makes the upper bound exclusive, so the
effective semantics are start-inclusive / end-exclusive at minute granularity.

## OpenGothic file:line

`/Users/admin/Downloads/opengothic/game/game/gamescript.cpp:1718` (`GameScript::wld_istime`)

```cpp
bool GameScript::wld_istime(int hour0, int min0, int hour1, int min1) {
  gtime begin{hour0,min0}, end{hour1,min1};
  gtime now = owner.time();
  now = gtime(0,now.hour(),now.minute());

  if(begin<=end && begin<=now && now<end)
    return true;
  else if(end<begin && (now<end || begin<=now))
    return true;
  else
    return 0;
  }
```

## Divergence

When `begin == end` (e.g. `Wld_IsTime(8,0,8,0)`), OpenGothic takes the
`begin<=end` branch and evaluates `begin<=now && now<end`. With `begin==end` this
is `begin<=now && now<begin`, which is **never true**. The original returns true
when `now == begin`. So OpenGothic answers `false` for the entire day, whereas
Gothic2.exe answers `true` for exactly the matching minute.

The non-equal cases match the original: OpenGothic's `begin<=now && now<end`
(non-wrap) and `now<end || begin<=now` (wrap) are exactly the original's
`end -= 1min` then inclusive-compare semantics at minute granularity.

This same root cause also affects routine selection, since `FindRoutine` selects
the active entry via `IsTimeBetween`: a zero-width routine window `[t,t]` is
active for one minute in the original but never in OpenGothic. The surgical fix
here targets only `wld_istime`; `Npc::currentRoutine` is left for a separate,
independently-verified change because it interacts with the existing
`r.start==r.end` special-casing in `Npc::endTime`.

## Proposed patch

Grep-verified symbols: `gtime` (`game/game/gametime.h`) provides
`operator==`, `operator<`, `operator<=`; `begin`, `end`, `now` are locals here.

OLD:
```cpp
  if(begin<=end && begin<=now && now<end)
    return true;
  else if(end<begin && (now<end || begin<=now))
    return true;
  else
    return 0;
  }
```

NEW:
```cpp
  // NOTE: in original-game oCWorldTimer::IsTimeBetween @0x00781190, the end-minute is
  // decremented (making the upper bound exclusive) only when begin!=end. For a zero-width
  // window begin==end the original returns true exactly when now==begin, not always false.
  if(begin==end)
    return now==begin;
  if(begin<end && begin<=now && now<end)
    return true;
  else if(end<begin && (now<end || begin<=now))
    return true;
  else
    return 0;
  }
```

(Note `begin<=end` is narrowed to `begin<end` in the first relational branch
since the `begin==end` case is now handled explicitly; behavior for `begin<end`
is unchanged.)
