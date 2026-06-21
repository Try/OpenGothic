# Mover SINGLE_KEYS NEXT/PREVIOUS does not wrap around

**Confidence:** High

## Original behavior

`zCMover::OnMessage` (Gothic2.exe, entry 0x00613450) handles `zCEventMover`
messages for movers whose behavior is SINGLE_KEYS (behavior field == 4).

- NEXT message (msgType 2): the target keyframe is computed as
  `(floor(currentPos) + 1) mod keyframeCount`. From the last keyframe this
  wraps around to keyframe 0.
- PREVIOUS message (msgType 3): the target keyframe is `floor(currentPos) - 1`;
  if that is negative, it is set to `keyframeCount - 1`. From keyframe 0 this
  wraps around to the last keyframe.

So in the original game NEXT/PREVIOUS on a SINGLE_KEYS mover form a cyclic ring
over all keyframes. (FIXED_DIRECT / FIXED_ORDER, msgTypes 0/1, simply clamp the
requested key to [0, count-1] — that part already matches OpenGothic.)

## OpenGothic divergence

`game/world/triggers/movetrigger.cpp:205-216` (`onGotoMsg`) maps NEXT to
`nextFrame(frame)` and PREVIOUS to `prevFrame(frame)`.

`nextFrame`/`prevFrame` (movetrigger.cpp:263-275) only wrap when behavior is
LOOP. SINGLE_KEYS is not LOOP, so:

- `nextFrame(last)` clamps and returns `size-1` (stays on the last keyframe)
- `prevFrame(0)` clamps and returns `0` (stays on the first keyframe)

Result: a SINGLE_KEYS mover driven by a MoverControler with NEXT/PREVIOUS gets
stuck at the ends instead of cycling. This affects mover-controller-driven
single-key movers (e.g. wheel/ring mechanisms) that rely on NEXT to loop back
to keyframe 0.

## Proposed patch

`game/world/triggers/movetrigger.cpp`

OLD:
```cpp
  switch(evt.move.msg) {
    case zenkit::MoverMessageType::NEXT:
      targetFrame = nextFrame(frame);
      break;
    case zenkit::MoverMessageType::PREVIOUS:
      targetFrame = prevFrame(frame);
      break;
```

NEW:
```cpp
  switch(evt.move.msg) {
    case zenkit::MoverMessageType::NEXT:
      // NOTE: in original-game (zCMover::OnMessage, 0x00613450) NEXT on a
      // SINGLE_KEYS mover wraps: target = (curFrame + 1) % keyframeCount.
      targetFrame = (frame + 1) % uint32_t(keyframes.size());
      break;
    case zenkit::MoverMessageType::PREVIOUS:
      // NOTE: in original-game PREVIOUS wraps: target = curFrame - 1, and if
      // negative it becomes keyframeCount - 1.
      targetFrame = (frame + uint32_t(keyframes.size()) - 1) % uint32_t(keyframes.size());
      break;
```

(`onGotoMsg` already guards `keyframes.size()<2` and `state!=Idle` above, so the
modulus is safe.)
