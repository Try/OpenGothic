# zCMover: transition (sfxMoving) sound is re-triggered per tick instead of looped + stopped on arrival

**Confidence:** High (divergence is certain). The proposed patch is a faithful reconstruction with one
explicitly documented API limitation (OpenGothic's `Sound` exposes no hard `stop()`, only `setLooping(false)`,
so the original's hard cut-on-arrival becomes a soft "finish current sample" stop).

## Original function + address (prose)

`zCMover::TriggerMover` (`Gothic2.exe` @0x00612cb0) and `zCMover::OnTick` (@0x00612f80) start the mover's
transition sound exactly **once**, at the instant the mover begins moving. The start-of-motion sound
(`sfxOpenStart`/`sfxCloseStart`, fields 0x1d0 / 0x20c) is played as a one-shot via the sound system's
one-shot entry (`zsound` vtable +0x2c). Immediately after, the *moving* sound `sfxMoving`
(zenkit `sfx_transitioning`, field 0x1a8) is played as a **looping** sound through a different entry
(`zsound` vtable +0x30, called with loop mode `2` and `zTSound3DParams`), and the returned playback
**handle is stored in field 0x1a4**.

`zCMover::AdvanceMover` (@0x00611d90) then **explicitly stops that handle** (`zsound` vtable +0x34 on field
0x1a4) the moment the mover reaches its end keyframe (the `0x194==1`/`0x194==3` arrival branches), and only
then plays the one-shot end sound (`sfxOpenEnd`/`sfxCloseEnd`, fields 0x1e4 / 0x220). So across the whole
move there is exactly one continuous, seamless loop of `sfxMoving`, hard-cut on arrival.

## OG file:line

`/Users/admin/Downloads/opengothic/game/world/triggers/movetrigger.cpp:331` (inside `MoveTrigger::tick`)
and `MoveTrigger::postProcessTrigger` (line 283). Header: `movetrigger.h` (member list, line ~65).

## Divergence

`MoveTrigger::tick()` calls `emitSound(sfxMoving);` on **every** tick while the mover is moving, with
`emitSound`'s default `freeSlot=true`. That creates a fresh **one-shot** `T_Regular` sound each time the
previous instance has finished (the `freeSlot` map guard at `interactive.cpp` only suppresses a restart
*while the prior copy is still playing*). Consequences vs the original:

1. **No seamless loop.** Samples designed to loop (stone-grind / lift hum / boat) are restarted from their
   attack transient each time they finish, producing audible clicks/gaps instead of one continuous loop.
2. **No hard stop on arrival.** OpenGothic never stops the moving sound; the last re-triggered one-shot
   plays out to its natural end *after* the mover has already stopped, overlapping the end sound. The
   original cuts it precisely at the arrival keyframe.

The constant/logic anchor is unambiguous: the original uses a distinct *looping* play call (vtbl +0x30,
loop mode 2) with a stored handle, plus a *stop-by-handle* call (vtbl +0x34) on arrival — neither of which
has any counterpart in the OpenGothic implementation.

## Proposed patch

Hold a single looping `Sound` for the lifetime of the move and stop it on arrival, instead of re-emitting a
one-shot every tick.

`movetrigger.h` — add include + member:

OLD
```cpp
#include "graphics/meshobjects.h"
#include "physics/physicmesh.h"
#include "abstracttrigger.h"
```
NEW
```cpp
#include "graphics/meshobjects.h"
#include "physics/physicmesh.h"
#include "abstracttrigger.h"
#include "world/objects/sound.h"
```

OLD
```cpp
    std::string                          sfxMoving;
    std::string                          visualName;
```
NEW
```cpp
    std::string                          sfxMoving;
    std::string                          visualName;
    Sound                                sfxMovingSlot;   // #mover looping transition sound
```

`movetrigger.cpp` — replace the per-tick one-shot in `tick()`:

OLD
```cpp
  advanceAnim();
  updateFrame();
  emitSound(sfxMoving);
  }
```
NEW
```cpp
  advanceAnim();
  updateFrame();
  // NOTE: in original-game zCMover::TriggerMover @0x00612cb0 / zCMover::OnTick @0x00612f80 the
  // transition sound (sfxMoving == sfx_transitioning, field 0x1a8) is started ONCE as a looping
  // sound (zsound vtable +0x30, loop mode 2; playback handle stored in field 0x1a4) and is hard-
  // stopped (handle 0x1a4, zsound vtable +0x34) in zCMover::AdvanceMover @0x00611d90 the moment the
  // mover reaches its end keyframe. OpenGothic re-emitted a fresh one-shot every tick, which restarts
  // the sample from its attack (audible clicks/gaps for grind/hum/boat sfx) and lets the last copy
  // overrun past arrival. Keep a single looping instance alive while moving.
  if(!sfxMoving.empty() && sfxMovingSlot.isFinished()) {
    sfxMovingSlot = ::Sound(world,::Sound::T_Regular,sfxMoving,position(),0,false);
    sfxMovingSlot.setLooping(true);
    sfxMovingSlot.play();
    }
  sfxMovingSlot.setPosition(position());
  }
```

`movetrigger.cpp` — stop the loop on arrival in `postProcessTrigger()`:

OLD
```cpp
void MoveTrigger::postProcessTrigger() {
  if(!target.empty()) {
```
NEW
```cpp
void MoveTrigger::postProcessTrigger() {
  // NOTE: original zCMover::AdvanceMover @0x00611d90 stops the looping transition sound (handle
  // 0x1a4) on arrival. OpenGothic's Sound has no hard stop(), so clear looping; WorldSound::tickSlot
  // then reclaims the slot once the current sample finishes (soft cut vs the original's hard cut).
  sfxMovingSlot.setLooping(false);
  if(!target.empty()) {
```

### Notes / limitations
- All APIs used exist and compile: `Sound()` default ctor, move-assign, `Sound(World&,Type,string_view,
  Vec3,float,bool)`, `setLooping`, `play`, `isFinished`, `setPosition` (`game/world/objects/sound.{h,cpp}`),
  and `position()` (`vob.h`). `freeSlot=false` routes the effect into `WorldSound::effect`, where a
  `loop && active` slot is kept replaying seamlessly and only reclaimed after `setLooping(false)`
  (`worldsound.cpp` `tickSlot`).
- `setLooping(false)` is a *soft* stop (current sample plays out), not the original's exact hard cut,
  because no `Sound::stop()` is exposed. This still removes the per-tick re-attack artifact and the
  open-ended overrun; a fully exact hard cut would require extending the `Sound` API.
- The reversal (`InvertMovement`) and `OPEN_TIME` stay-open paths are unaffected: reversal stays mid-move
  so the existing loop keeps playing; the stay-open pause is reached via `postProcessTrigger`
  (frame==targetFrame), which already stops the loop before the `OpenTimed` idle.
- Distinct from the already-applied/deferred mover items (reverse-midmotion, SINGLE_KEYS wrap,
  TRIGGER_CONTROL ref-count, OPEN_TIME re-trigger, untrigger window, carry drop-ray cache, easing curve).
