# Mover keyframe easing: trigonometric (original) vs polynomial (OpenGothic)

**Confidence:** High

## Original function + address

`zCMover::InterpolateKeyframes_KF` (Gothic2.exe `0x00611900`). This routine computes the
intra-segment interpolation parameter `t` (the fractional part of the global keyframe position
`framePos`, field at vob+0x180) and then *reshapes* it according to the mover's speed/easing mode
(field at vob+0x1cc, which is `moverSpeedType`). The reshaping is done with the engine's
look-up-table trigonometric helpers `zSinApprox` / `zCosApprox` (`zSinApprox` @ a `zTools.cpp`
sine table), which approximate true `sin`/`cos`:

- mode 4 / "slow start+end" curve: `t = 0.5 * (sin(t*PI - PI/2) + 1)` = `0.5 * (1 - cos(t*PI))`
- mode 5 / "slow start" curve: `t = 1 - cos(t*PI/2)`
- mode 6 / "slow end" curve: `t = sin(t*PI/2)`

Mode dispatch (field vob+0x1cc): `0`=CONSTANT (no reshape); `1`=SLOW_START_END applies the
smoothstep curve when the segment is both first and last, else the start curve in the first
segment and the end curve in the last segment; `2`=SLOW_START applies the start curve only in the
first segment; `3`=SLOW_END applies the end curve only in the last segment; `4/5/6`=SEGMENT_*
apply the corresponding curve on every segment. "First/last segment" is decided from `framePos`
(`framePos < 1.0` for first, `framePos > numKeyframes-2` for last). For `moverBehavior == LOOP`
(field vob+0x1c0 == 3) all easing is skipped (constant speed). Float constants verified:
`0x40490fdb`=PI, `0x3fc90fdb`=PI/2.

This dispatch structure (per-segment start/end detection, SEGMENT variants always on, CONSTANT and
LOOP bypass) matches OpenGothic's `calcProgress` exactly — only the curve *shape* differs.

## OpenGothic file:line

`game/world/triggers/movetrigger.cpp:332-363` (`MoveTrigger::calcProgress`).

## Divergence

OpenGothic reshapes the interpolation parameter `a` with cubic-polynomial approximations instead
of the original's trigonometric curves:

| Curve | Original (`InterpolateKeyframes_KF`) | OpenGothic `calcProgress` |
|-------|--------------------------------------|---------------------------|
| smoothstep (start+end / SEGMENT_SLOW_START_END) | `0.5*(1 - cos(a*PI))` | `(3 - 2a)*a*a` |
| slow start (SLOW_START / SEGMENT_SLOW_START)     | `1 - cos(a*PI/2)`      | `(2 - a)*a*a` |
| slow end (SLOW_END / SEGMENT_SLOW_END)           | `sin(a*PI/2)`          | `((1-a)*a + 1)*a` |

The endpoints (a=0, a=1) agree, so a mover still starts and stops at the same keyframes after the
same total segment time. But the *within-segment* position diverges: the smoothstep curves differ
by up to ~1% of the segment, while the slow-start and slow-end curves differ by up to ~9% of the
segment (peak near a~0.5..0.6). For an elevator/platform or a slow door this is a visible timing
mismatch in where the platform is partway through a segment — relevant to the "move-trigger"
glide/timing complaints (issues #637/#623). The original's `zSinApprox`/`zCosApprox` are merely a
LUT optimization of true `sin`/`cos`, so `std::sin`/`std::cos` is a faithful (slightly more
accurate) reimplementation.

## Proposed patch

`std::sin`, `std::cos`, `M_PI` and `std::clamp` are all already used in this file (e.g.
`movetrigger.cpp:41` uses `M_PI`). Symbols `speedType`, the `MoverSpeedType` enum members, and the
`start`/`end` locals are grep-verified present.

OLD (`game/world/triggers/movetrigger.cpp`, inside `calcProgress`):
```cpp
  switch(speedType) {
    case zenkit::MoverSpeedType::CONSTANT:
      break;
    case zenkit::MoverSpeedType::SLOW_START_END:
    case zenkit::MoverSpeedType::SEGMENT_SLOW_START_END:
      if((start && end) || speedType==zenkit::MoverSpeedType::SEGMENT_SLOW_START_END)
        a = (3 - 2*a) * a*a;
      else if(start)
        a = (2 - a) * a*a;
      else if(end)
        a = ((1 - a)*a + 1)*a;
      break;
    case zenkit::MoverSpeedType::SLOW_START:
    case zenkit::MoverSpeedType::SEGMENT_SLOW_START:
      if(start || speedType==zenkit::MoverSpeedType::SEGMENT_SLOW_START)
        a = (2 - a) * a*a;
      break;
    case zenkit::MoverSpeedType::SLOW_END:
    case zenkit::MoverSpeedType::SEGMENT_SLOW_END:
      if(end || speedType==zenkit::MoverSpeedType::SEGMENT_SLOW_END)
        a = ((1 - a)*a + 1)*a;
      break;
    }
```

NEW:
```cpp
  // NOTE: in original-game zCMover::InterpolateKeyframes_KF (Gothic2.exe 0x00611900) the
  // intra-segment easing is sinusoidal, not polynomial: smoothstep = 0.5*(1-cos(a*PI)),
  // slow-start = 1-cos(a*PI/2), slow-end = sin(a*PI/2). zSinApprox/zCosApprox are LUT sin/cos,
  // so std::sin/std::cos reproduce the curves. Endpoints match the old polynomials, but the
  // mid-segment shape differed (slow-start/slow-end by up to ~9% of the segment).
  const float smoothStep = 0.5f*(1.f - std::cos(a*float(M_PI)));
  const float slowStart  = 1.f - std::cos(a*float(M_PI)*0.5f);
  const float slowEnd    = std::sin(a*float(M_PI)*0.5f);
  switch(speedType) {
    case zenkit::MoverSpeedType::CONSTANT:
      break;
    case zenkit::MoverSpeedType::SLOW_START_END:
    case zenkit::MoverSpeedType::SEGMENT_SLOW_START_END:
      if((start && end) || speedType==zenkit::MoverSpeedType::SEGMENT_SLOW_START_END)
        a = smoothStep;
      else if(start)
        a = slowStart;
      else if(end)
        a = slowEnd;
      break;
    case zenkit::MoverSpeedType::SLOW_START:
    case zenkit::MoverSpeedType::SEGMENT_SLOW_START:
      if(start || speedType==zenkit::MoverSpeedType::SEGMENT_SLOW_START)
        a = slowStart;
      break;
    case zenkit::MoverSpeedType::SLOW_END:
    case zenkit::MoverSpeedType::SEGMENT_SLOW_END:
      if(end || speedType==zenkit::MoverSpeedType::SEGMENT_SLOW_END)
        a = slowEnd;
      break;
    }
```

The surrounding `std::clamp(a,0.f,1.f)` on entry and exit is unchanged, and the `start`/`end`
detection is unchanged (it already matches the original's first/last-segment test). `<cmath>` is
transitively available (file already calls `std::acos`/`std::pow`/`M_PI`).

### Note on a related, separately-deferred nuance
The original skips *all* easing when `moverBehavior == LOOP` (vob+0x1c0 == 3), including the
SEGMENT_* modes. OpenGothic's `calcProgress` zeroes the `start`/`end` flags for `state==Loop` but
still applies the SEGMENT_* curves unconditionally. Whether to also suppress SEGMENT_* easing
during LOOP is a behavior question I am **not** folding into this patch (DEFERRED: needs in-game
confirmation that a SEGMENT_* + LOOP mover exists and that constant-speed is the desired vanilla
result; low risk either way, out of scope for the easing-curve fix).
