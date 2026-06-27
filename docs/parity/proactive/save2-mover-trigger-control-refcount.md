# Save/Load parity: TRIGGER_CONTROL mover ref-count is dropped across save/load

**Confidence:** High

## Original function + address (prose only)

The original `zCMover` keeps a per-mover *trigger reference count* for movers whose
behavior is `TRIGGER_CONTROL`. In `zCMover::TriggerMover` (Gothic2.exe @0x00612cb0) the
behavior==`TRIGGER_CONTROL` branch increments this counter on every accepted OnTrigger
(`field_0x198 += 1`), and only begins opening when the mover is currently in its closed/stand
state. In `zCMover::OnUntrigger` (Gothic2.exe @0x00613170) the same branch decrements the
counter (`field_0x198 -= 1`) and only starts the close/invert movement once the decremented
value drops below 1 — i.e. once *all* sources that triggered the mover have untriggered.

Crucially, this counter is part of the persisted save state: `zCMover::Archive`
(Gothic2.exe @0x006137e0) writes it inside the savegame-only block
(`InSaveGame()` gate, vtbl slot 0x100) via `WriteInt(field_0x198)`, alongside the runtime
keyframe/position/state fields, and `zCMover::Unarchive` (@0x00613aa0) reads it back. So a
`TRIGGER_CONTROL` mover that was opened by N independent triggers and then saved restores
with the ref-count = N, and continues to require N untriggers before it closes.

## OpenGothic file:line

- `game/world/triggers/movetrigger.cpp:71` (`MoveTrigger::save`)
- `game/world/triggers/movetrigger.cpp:76` (`MoveTrigger::load`)
- `game/world/triggers/movetrigger.h:73` (`int triggerCount = 0;`)
- `game/world/triggers/movetrigger.cpp:161-199` (`onTrigger`/`onUntrigger`, which already
  implement the increment/decrement ref-count semantics)

## Divergence

OpenGothic already models the ref-count correctly at runtime: `onTrigger` does
`++triggerCount` for `TRIGGER_CONTROL`, and `onUntrigger` does the matching decrement and
only closes when `triggerCount` reaches 0. But `MoveTrigger::save`/`MoveTrigger::load`
persist only `pos0, state, frameTime, frame, targetFrame` — `triggerCount` is **not**
serialized. On load it defaults to 0.

Consequence: a `TRIGGER_CONTROL` mover opened by 2+ sources, saved while open, then loaded,
will close on the *first* untrigger instead of the last — diverging from the original, which
restores the full ref-count. The existing NOTE at `movetrigger.cpp:162-165` explicitly
acknowledges this drop and rationalizes it as "matches prior behavior — no stuck state", but
it *is* a behavioral divergence from `Gothic2.exe`, which persists the counter.

## Proposed patch

Bump the serialize version, persist `triggerCount`, and guard the load. `Serialize::Current`
is currently `56` (`game/game/serialize.h:36`); bump to `57`.

`game/game/serialize.h` (OLD):
```cpp
      Current    = 56, // 56: persist Npc::aiOutputBarrier
```
(NEW):
```cpp
      Current    = 57, // 57: persist MoveTrigger::triggerCount (TRIGGER_CONTROL ref count)
```

`game/world/triggers/movetrigger.cpp` `MoveTrigger::save` (OLD):
```cpp
void MoveTrigger::save(Serialize& fout) const {
  AbstractTrigger::save(fout);
  fout.write(pos0,uint8_t(state),frameTime,frame,targetFrame);
  }
```
(NEW):
```cpp
void MoveTrigger::save(Serialize& fout) const {
  AbstractTrigger::save(fout);
  fout.write(pos0,uint8_t(state),frameTime,frame,targetFrame);
  // NOTE: in original-game zCMover::Archive (Gothic2.exe @0x006137e0) the TRIGGER_CONTROL
  // ref count (field_0x198, incremented in TriggerMover @0x00612cb0, decremented in
  // OnUntrigger @0x00613170) is persisted in the savegame block; restore it so a mover
  // opened by several sources still requires all of them to untrigger before closing.
  fout.write(triggerCount);
  }
```

`game/world/triggers/movetrigger.cpp` `MoveTrigger::load` (OLD):
```cpp
void MoveTrigger::load(Serialize& fin) {
  AbstractTrigger::load(fin);
  fin.read(pos0,reinterpret_cast<uint8_t&>(state),frameTime,frame);
  if(fin.version()>49)
    fin.read(targetFrame);
  if(state!=Idle) {
    invalidateView();
    enableTicks();
    }
  }
```
(NEW):
```cpp
void MoveTrigger::load(Serialize& fin) {
  AbstractTrigger::load(fin);
  fin.read(pos0,reinterpret_cast<uint8_t&>(state),frameTime,frame);
  if(fin.version()>49)
    fin.read(targetFrame);
  if(fin.version()>56)
    fin.read(triggerCount);
  if(state!=Idle) {
    invalidateView();
    enableTicks();
    }
  }
```

Also update the stale rationale comment at `movetrigger.cpp:162-165`, which currently states
the counter is "Not persisted across save/load here"; that line is no longer accurate after
this change.

Grep-verified symbols: `triggerCount` (`movetrigger.h:73`, `int`, default 0);
`Serialize::Current` (`serialize.h:36`); `Serialize::version()`/`read`/`write` already used in
this file with the `fin.version()>49` guard pattern.
