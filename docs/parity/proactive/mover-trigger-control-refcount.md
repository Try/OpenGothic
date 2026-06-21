# TRIGGER_CONTROL mover ignores trigger reference count

**Confidence:** Medium

## Original behavior

For a mover with behavior TRIGGER_CONTROL (behavior field == 1) the original
engine keeps an integer trigger counter at offset 0x198:

- `zCMover::TriggerMover` (Gothic2.exe 0x00612cb0), behavior==1 branch:
  increments the counter (`count++`) on every OnTrigger, and only actually
  starts the open motion when the mover is in the closed-finished state.
- `zCMover::OnUntrigger` (0x00613170), behavior==1 branch: **decrements** the
  counter and only begins closing when the counter falls below 1
  (`if(--count < 1)`).

So a TRIGGER_CONTROL mover fed by several triggers stays open until every one
of those sources has sent its OnUntrigger. The first untrigger of N does not
close it.

## OpenGothic divergence

`game/world/triggers/movetrigger.cpp`:

- `onTrigger`, TRIGGER_CONTROL case (lines 161-165): opens on the first
  trigger, no counter.
- `onUntrigger` (lines 183-193): closes on the first untrigger
  (`if(frame>0 && state==Idle)`), with no counter.

There is no reference count, so a single OnUntrigger closes the mover even when
multiple trigger sources had opened it. With several triggers wired to one
TRIGGER_CONTROL mover (logic-gate / multi-plate setups), OpenGothic closes the
mover prematurely compared with the original.

## Proposed patch

Add a counter and gate open/close on it. `game/world/triggers/movetrigger.h`
needs a member (add near the other mover state fields):

```cpp
  int triggerCount = 0;   // NOTE: original-game offset 0x198, TRIGGER_CONTROL ref count
```

`game/world/triggers/movetrigger.cpp`

OLD:
```cpp
    case zenkit::MoverBehavior::TRIGGER_CONTROL: {
      if(frame==0 && state==Idle)
        state = Open;
      break;
      }
```

NEW:
```cpp
    case zenkit::MoverBehavior::TRIGGER_CONTROL: {
      // NOTE: in original-game (zCMover::TriggerMover, 0x00612cb0) each
      // OnTrigger increments a ref count; close only happens once all sources
      // have untriggered.
      ++triggerCount;
      if(frame==0 && state==Idle)
        state = Open;
      break;
      }
```

OLD:
```cpp
  if(behavior!=zenkit::MoverBehavior::TRIGGER_CONTROL)
    return;
  if(frame>0 && state==Idle) {
    state       = Close;
    targetFrame = 0;
    preProcessTrigger();
    }
```

NEW:
```cpp
  if(behavior!=zenkit::MoverBehavior::TRIGGER_CONTROL)
    return;
  // NOTE: in original-game (zCMover::OnUntrigger, 0x00613170) the close only
  // begins when the trigger ref count drops below 1.
  if(triggerCount>0)
    --triggerCount;
  if(triggerCount>0)
    return;
  if(frame>0 && state==Idle) {
    state       = Close;
    targetFrame = 0;
    preProcessTrigger();
    }
```

(`triggerCount` should also be added to `save`/`load` for save-game parity.)
