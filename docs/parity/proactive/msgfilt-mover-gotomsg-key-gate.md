# MoverControler dispatch: onGotoMsg drops out-of-range key instead of clamping/ignoring it

**Confidence:** Medium-High

## Original behavior

`zCMoverControler::OnTrigger` (Gothic2.exe, entry 0x00619fc0) builds a
`zCEventMover` carrying the configured message type (field at +0x24, from
`zCMoverControler+0x134`) and the goto key (field at +0x2c, from
`zCMoverControler+0x138`) and sends it to every target mover's event manager.

The consumer is `zCMover::OnMessage` (Gothic2.exe, entry 0x00613450). For a
`zCEventMover` it switches on the message type and is gated only by
*behavior == 4 (SINGLE_KEYS)* and *current move-speed == 0 (idle)*. The
`key` field is used **only** on the FIXED paths, and there it is **clamped**,
never used to drop the message:

- case 0 `FIXED_DIRECT`: `key` is clamped into `[0, keyframeCount-1]`
  (`if(k<0) k=0; else if(k>count-1) k=count-1;`), then `SetToKeyframe`.
- case 1 `FIXED_ORDER`: `zClamp(key, 0, keyframeCount-1)`, then `SetToKeyframe`.
- case 2 `NEXT`: target = `(floor(curFrame)+1) % keyframeCount` — `key` is
  never read.
- case 3 `PREVIOUS`: target = `floor(curFrame)-1`, or `keyframeCount-1` if
  negative — `key` is never read.

So the original never discards a mover message because of `key`: FIXED keys are
clamped into range, and NEXT/PREVIOUS ignore `key` entirely. This matches the
ZenKit field doc: `VMoverController::key` is "only relevant if message is
FIXED_DIRECT or FIXED_ORDER" (`lib/ZenKit/include/zenkit/vobs/Misc.hh:314-317`),
i.e. for NEXT/PREVIOUS the field is commonly left at an unused sentinel such as
`-1`.

## OpenGothic divergence

`game/world/triggers/movetrigger.cpp:221-222` (`onGotoMsg`):

```cpp
  if(evt.move.key<0 || keyframes.size()<size_t(evt.move.key))
    return;
```

This blanket guard runs for **every** message type, before the
NEXT/PREVIOUS/FIXED switch. Because `evt.move.key == int(uint32_t(ctrl.key))`
(`movercontroler.cpp:9,17-18`), a MoverControler whose `key` is the unused
sentinel (`-1`) or out of range makes `onGotoMsg` return immediately:

- **NEXT / PREVIOUS with key = -1 (or any out-of-range value):** the original
  steps/wraps the mover ignoring `key`; OpenGothic drops the message and the
  mover never moves. This is the typical authoring case for cyclic single-key
  mechanisms (wheels/rings), which leave `gotoFixedKey` unset.
- **FIXED_DIRECT / FIXED_ORDER with key out of `[0,count-1]`:** the original
  clamps and still fires; OpenGothic drops it (key `> count`) or sets an
  out-of-bounds `targetFrame` (key `== count`, since the guard uses `<` not
  `<=`).

The pre-existing `mover-singlekeys-nextprev-wrap.md` fixed only the NEXT/PREVIOUS
wrap *math*; it explicitly (and incorrectly) assumed "FIXED_DIRECT / FIXED_ORDER
simply clamp ... already matches OpenGothic" and did not touch this key gate.

## Proposed patch

`game/world/triggers/movetrigger.cpp`

OLD:
```cpp
void MoveTrigger::onGotoMsg(const TriggerEvent& evt) {
  if(keyframes.size()<2 || keyframes[0].ticks==0)
    return;
  if(evt.move.key<0 || keyframes.size()<size_t(evt.move.key))
    return;
  if(behavior!=zenkit::MoverBehavior::SINGLE_KEYS)
    return;
  if(state!=Idle)
    return;
  state = SingleKey;
  switch(evt.move.msg) {
    case zenkit::MoverMessageType::NEXT:
      // NOTE: in original-game zCMover::OnMessage (Gothic2.exe 0x00613450) NEXT on a
      // SINGLE_KEYS mover wraps: target = (curFrame+1) % keyframeCount (nextFrame only wraps
      // for LOOP behavior, so it got stuck on the last keyframe).
      targetFrame = (frame + 1) % uint32_t(keyframes.size());
      break;
    case zenkit::MoverMessageType::PREVIOUS:
      // NOTE: original PREVIOUS wraps: target = curFrame-1, or keyframeCount-1 if negative.
      targetFrame = (frame + uint32_t(keyframes.size()) - 1) % uint32_t(keyframes.size());
      break;
    case zenkit::MoverMessageType::FIXED_DIRECT:
    case zenkit::MoverMessageType::FIXED_ORDER:
      targetFrame = uint32_t(evt.move.key);
      break;
    }
  preProcessTrigger();
  }
```

NEW:
```cpp
void MoveTrigger::onGotoMsg(const TriggerEvent& evt) {
  if(keyframes.size()<2 || keyframes[0].ticks==0)
    return;
  if(behavior!=zenkit::MoverBehavior::SINGLE_KEYS)
    return;
  if(state!=Idle)
    return;
  state = SingleKey;
  switch(evt.move.msg) {
    case zenkit::MoverMessageType::NEXT:
      // NOTE: in original-game zCMover::OnMessage @0x00613450 NEXT/PREVIOUS ignore the message's
      // gotoFixedKey entirely (it is "only relevant" for FIXED_*). OpenGothic's blanket
      // `evt.move.key` range gate dropped NEXT/PREVIOUS whenever key was the usual unused
      // sentinel (-1), so the mover never stepped. NEXT wraps: target = (curFrame+1) % count.
      targetFrame = (frame + 1) % uint32_t(keyframes.size());
      break;
    case zenkit::MoverMessageType::PREVIOUS:
      // NOTE: original PREVIOUS wraps: target = curFrame-1, or keyframeCount-1 if negative.
      targetFrame = (frame + uint32_t(keyframes.size()) - 1) % uint32_t(keyframes.size());
      break;
    case zenkit::MoverMessageType::FIXED_DIRECT:
    case zenkit::MoverMessageType::FIXED_ORDER: {
      // NOTE: in original-game zCMover::OnMessage @0x00613450 the FIXED_* paths CLAMP gotoFixedKey
      // into [0,keyframeCount-1] (zClamp) and still fire; they never drop an out-of-range key.
      int32_t k = evt.move.key;
      if(k<0)
        k = 0;
      else if(size_t(k)>=keyframes.size())
        k = int32_t(keyframes.size())-1;
      targetFrame = uint32_t(k);
      break;
      }
    }
  preProcessTrigger();
  }
```
