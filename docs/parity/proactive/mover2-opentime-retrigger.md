# Mover OPEN_TIME re-trigger: OpenGothic extends stay-open, original closes

**Confidence:** High

## Original function + address

`zCMover::TriggerMover` (Gothic2.exe @0x00612cb0) is the central reaction
to a mover being triggered. For behavior `OPEN_TIME` (the internal behavior
field at offset 0x1c0 holds value 2), it branches on the mover's internal
motion-state field (offset 0x194; 0 = fully open/at-rest, 1 = opening,
2 = fully closed/at-rest, 3 = closing):

- If the mover is closed (state 2): begin opening (state := 1, set keyframe 0,
  direction +1, play open-start sound).
- If the mover is fully open and at rest (state 0):
  - If the stay-open countdown timer (offset 0x19c) is already running
    (non-zero), it is overwritten with the constant `1.0f` (0x3f800000).
  - Otherwise the countdown is (re)started to `stayOpenSeconds * 1000.0f`
    (0x1b0 * 1000.0, plus a tiny wake epsilon DAT_0099b3dc).

The countdown counts down each frame; `zCMover::OnTick` (@0x00612f80) closes the
mover once `0x19c <= epsilon` by switching to state 3 and stepping back from the
last keyframe (`SetToKeyframe(numKeyframes-1, -1.0)`). The stay-open value is
installed by `zCMover::FinishedOpening` (@0x006126f0) / the inlined finish path
in `SetToKeyframe_KF` (@0x00611400), which for `OPEN_TIME` sets
`0x19c = stayOpenSeconds*1000 + epsilon` and keeps the vob awake.

Net effect in the original: **re-triggering an OPEN_TIME mover while it is
fully open and waiting forces it to close almost immediately** (the countdown is
collapsed to ~1 ms, so it expires on the next tick). The countdown is only set
to its full duration when it is not already running (i.e. right after opening
completes).

## OpenGothic file:line

`game/world/triggers/movetrigger.cpp:177-181` (`MoveTrigger::onTrigger`,
`OPEN_TIME` case):

```cpp
case zenkit::MoverBehavior::OPEN_TIME: {
  if(state==OpenTimed)
    frameTime = 0; else
    state     = Open;
  break;
  }
```

The stay-open expiry itself lives in `MoveTrigger::tick` at
`movetrigger.cpp:302-308`:

```cpp
if(state==OpenTimed) {
  if(frameTime<=stayOpenTime)
    return;
  state       = Close;
  targetFrame = 0;
  frameTime  -= stayOpenTime;
  }
```

## Divergence

When an `OPEN_TIME` mover is fully open and waiting (OpenGothic state
`OpenTimed`), OpenGothic handles a fresh trigger by setting `frameTime = 0`,
which **restarts the full stay-open countdown** — the mover stays open another
whole `stayOpenTime`. The original does the opposite: it collapses the countdown
so the mover **closes on the next tick**. With a source that re-triggers
repeatedly (e.g. an NPC/plate that keeps firing the trigger while standing on
it), OpenGothic can hold such a mover open indefinitely, whereas the original
would close it. This is a behavior/timing-logic divergence distinct from the
already-fixed NEXT/PREV wrap, TRIGGER_CONTROL ref-count, sinusoidal easing and
ref-count save.

## Proposed patch

OLD (`movetrigger.cpp:177-181`):
```cpp
    case zenkit::MoverBehavior::OPEN_TIME: {
      if(state==OpenTimed)
        frameTime = 0; else
        state     = Open;
      break;
      }
```

NEW:
```cpp
    case zenkit::MoverBehavior::OPEN_TIME: {
      // NOTE: in original-game zCMover::TriggerMover (Gothic2.exe @0x00612cb0) a re-trigger of
      // a fully-open OPEN_TIME mover collapses the stay-open countdown (field_0x19c) to ~1ms so
      // the mover closes on the next tick (zCMover::OnTick @0x00612f80), rather than restarting
      // the full duration. Forcing frameTime past stayOpenTime makes the OpenTimed branch in
      // tick() transition to Close on the next tick, reproducing that "re-trigger closes" behavior.
      if(state==OpenTimed)
        frameTime = stayOpenTime; else
        state     = Open;
      break;
      }
```

`frameTime` and `stayOpenTime` are existing `MoveTrigger` members
(grep-verified in `movetrigger.h`); `state==OpenTimed` is the existing
internal `State` enum value. Setting `frameTime = stayOpenTime` makes the next
`tick()` see `frameTime += dt > stayOpenTime`, transition to `Close` with
`frameTime -= stayOpenTime` leaving a small residual (`~dt`), i.e. an immediate
clean close — matching the original's ~1 ms countdown collapse.
