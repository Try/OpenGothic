# Mover reversal mid-motion shifts to the wrong segment and freezes

**Confidence:** High

## Original fn + address

`zCMover::TriggerMover` (Gothic2.exe @0x00612cb0) is the re-trigger entry point. For a
2STATE / TOGGLE mover (behaviour field_0x1c0 == 0) it inspects the live move-state
field_0x194: the branches only fully handle `0x194 == 2` (fully open) and `0x194 == 0`
(fully closed). When the mover is *currently moving* (`0x194 == 1` opening, `0x194 == 3`
closing) every branch falls through to the tail call `zCMover::InvertMovement`
(@0x00612300). InvertMovement swaps the open/close state (0x194: 1↔3) and calls
`SetToKeyframe(actAniFrame /*field_0x180*/, -direction /*field_0x190*/)`, i.e. it keeps the
**same continuous keyframe position** and merely negates the per-tick frame velocity. The
mover therefore reverses *in place*, staying inside its current keyframe segment with a
continuous world position — it never jumps to an adjacent segment.

`zCMover::AdvanceMover` @0x00611d90 / `SetToKeyframe_KF` @0x00611400 confirm the model:
position is `keyframe[beg] + frac(actAniFrame)·(keyframe[end]-keyframe[beg])` with
`beg = floor` when opening and `beg = ceil` when closing, so a sign flip of field_0x190 at
a fixed actAniFrame yields an identical world position.

## OG file:line

`/Users/admin/Downloads/opengothic/game/world/triggers/movetrigger.cpp:247-270`
(`MoveTrigger::preProcessTrigger`), the two `if(prev==Close)` / `if(prev==Open)` reversal
fast-paths.

## Divergence

OpenGothic reimplements the continuous-actAniFrame original with a discrete
`frame` + `frameTime` model where, during **Open**, `frame` is the *lower* index of the
active segment (`advanceAnim`: `f1 = frame`, `updateFrame`: `frame = nextFrame(frame)`),
and during **Close**, `frame` is the *upper* index (`f1 = prevFrame(frame)`,
`updateFrame`: `frame = prevFrame(frame)`).

When a moving TOGGLE mover is re-triggered (`onTrigger` swaps `Open↔Close`),
`preProcessTrigger` must keep the mover in the **same** segment, only swapping which end
index `frame` names. Reversing Close→Open must set `frame = prevFrame(frame)` (upper→lower
of the same segment); reversing Open→Close must set `frame = nextFrame(frame)`
(lower→upper of the same segment). The current code does the opposite — it moves `frame`
into an **adjacent** segment:

- Close→Open does `frame = nextFrame(frame)`.
- Open→Close does `frame = prevFrame(frame)`.

Worked example (3 keyframes, equal segment ticks T): a mover closing through segment
`[1,2]` at 70% (frame=2, frameTime=0.3T) is re-triggered to open. Correct result keeps it
at 70% of `[1,2]` and continues to kf2. The current code instead sets frame=`nextFrame(2)`
=2 (clamped) with targetFrame=2, so on the very next `tick()` `frame==targetFrame` fires
`postProcessTrigger()` immediately: the mover **freezes at 70% and goes Idle** instead of
finishing its travel. The symmetric Open→Close case freezes the same way (frame collapses
to targetFrame 0). Net effect: re-triggering any 2STATE mover while it is still
opening/closing snaps it to a stop at a partial position, where the original smoothly
reverses. (The frameTime base is also taken from the wrong segment's `ticks[]` when
segments have unequal lengths.)

The two branch bodies are simply **swapped** relative to the correct behaviour — the
existing Open-branch body is the correct Close-branch body and vice-versa.

## Proposed patch

```cpp
// OLD  (movetrigger.cpp, preProcessTrigger)
  if(state==Open) {
    targetFrame = uint32_t(keyframes.size())-1;
    emitSound(sfxOpenStart);
    if(prev==Close) {
      frameTime = keyframes[frame].ticks - frameTime;
      frame     = nextFrame(frame);
      return;
      }
    }
  else if(state==Close) {
    targetFrame = 0;
    emitSound(sfxCloseStart);
    if(prev==Open) {
      frame     = prevFrame(frame);
      frameTime = keyframes[frame].ticks - frameTime;
      return;
      }
    }
```

```cpp
// NEW
  if(state==Open) {
    targetFrame = uint32_t(keyframes.size())-1;
    emitSound(sfxOpenStart);
    if(prev==Close) {
      // NOTE: in original-game zCMover::InvertMovement @0x00612300 (reached from
      // zCMover::TriggerMover @0x00612cb0 when a 2STATE mover is re-triggered while
      // moving) reversal negates the per-tick frame velocity at the *same* continuous
      // actAniFrame, keeping the mover inside its current segment. Reversing Close->Open
      // must stay in segment [prevFrame(frame),frame]; for Open the lower index is
      // prevFrame(frame). The frameTime base is that segment's own ticks.
      frame     = prevFrame(frame);
      frameTime = keyframes[frame].ticks - frameTime;
      return;
      }
    }
  else if(state==Close) {
    targetFrame = 0;
    emitSound(sfxCloseStart);
    if(prev==Open) {
      // NOTE: see zCMover::InvertMovement @0x00612300 — reversing Open->Close stays in
      // segment [frame,nextFrame(frame)]; for Close the upper index is nextFrame(frame)
      // and the segment ticks are keyframes[frame].ticks (read before reassigning frame).
      // The previous code dropped into an adjacent segment, so frame==targetFrame fired
      // immediately and froze the re-triggered mover mid-travel.
      frameTime = keyframes[frame].ticks - frameTime;
      frame     = nextFrame(frame);
      return;
      }
    }
```

Distinct from the existing mover docs (easing curve, SINGLE_KEYS next/prev wrap,
TRIGGER_CONTROL ref-count, OPEN_TIME re-trigger, untrigger retrigger window): those leave
this TOGGLE mid-motion reversal frame math untouched.
