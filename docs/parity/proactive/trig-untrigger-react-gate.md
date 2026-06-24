# zCTrigger: OnUntrigger/OnUntouch must be gated by reactToOnTrigger / reactToOnTouch

**Confidence:** High

## Original function + address

`zCTrigger::OnUntrigger` (address `0x00610600`) and `zCTrigger::OnUntouch`
(address `0x00610660`) in `Gothic2.exe`.

In the original engine these two handlers are gated *before* anything else by the
corresponding "react" filter bit on the trigger flag byte (offset `+0x134`):

- `OnUntrigger` first tests bit0 (`react_to_on_trigger`). Only if it is set does it
  go on to require `IsOnTimer()==0`, the runtime-enabled bit (`+0x135` bit1), and the
  `send_untrigger` bit (`+0x135` bit2) before forwarding the untrigger to the target.
- `OnUntouch` is identical but tests bit1 (`react_to_on_touch`) instead.

The flag-byte bit layout is confirmed by `zCTrigger::Unarchive` (`0x00610a60`) /
`zCTrigger::Archive` (`0x006107e0`), which serialize the bits in order
react_to_on_trigger(bit0), react_to_on_touch(bit1), react_to_on_damage(bit2),
respond_to_object(bit3), respond_to_pc(bit4), respond_to_npc(bit5). The behavior is
also documented in ZenKit's `VTrigger::send_untrigger`:
"Only fires the OnUntrigger event if react_to_on_trigger and react_to_on_touch are
set to TRUE respectively."

## OpenGothic file:line

`game/world/triggers/abstracttrigger.cpp:109-113` (the `T_Untrigger` case of
`AbstractTrigger::implProcessEvent`).

```cpp
case TriggerEvent::T_Untrigger:
  if(disabled || !sendUntrigger)
    return;
  onUntrigger(evt);
  break;
```

## Divergence

OpenGothic forwards an untrigger to the target whenever the trigger is enabled and
`sendUntrigger` is set, **regardless of the `reactToOnTrigger` flag**. The original
engine ignores `OnUntrigger` entirely on a trigger whose `react_to_on_trigger` is
false (and ignores `OnUntouch` when `react_to_on_touch` is false).

A concrete divergent case: a touch-only trigger (`react_to_on_trigger = false`,
`react_to_on_touch = true`, `send_untrigger = true`, `start_enabled = true`) that
receives an `OnUntrigger` script message. The original drops it; OpenGothic forwards
an untrigger to the target. (Note OpenGothic collapses untouch into the same
`T_Untrigger` path, so the touch-side `reactToOnTouch` gate is also absent.)

Both `reactToOnTrigger` and `reactToOnTouch` already exist as members of
`AbstractTrigger` (`abstracttrigger.h:92-93`) and are populated from
`react_to_on_trigger` / `react_to_on_touch` in the constructor
(`abstracttrigger.cpp:43-44`), so the gate can be added without new state.

## Proposed patch

OLD (`game/world/triggers/abstracttrigger.cpp`):
```cpp
    case TriggerEvent::T_Untrigger:
      if(disabled || !sendUntrigger)
        return;
      onUntrigger(evt);
      break;
```

NEW:
```cpp
    case TriggerEvent::T_Untrigger:
      // NOTE: in original-game zCTrigger::OnUntrigger @0x00610600 (and OnUntouch
      // @0x00610660) an untrigger/untouch is dropped unless the matching react flag
      // is set: OnUntrigger requires react_to_on_trigger, OnUntouch requires
      // react_to_on_touch. OpenGothic merges both into T_Untrigger, so require
      // either react flag here.
      if(disabled || !sendUntrigger)
        return;
      if(!reactToOnTrigger && !reactToOnTouch)
        return;
      onUntrigger(evt);
      break;
```

Rationale for `(!reactToOnTrigger && !reactToOnTouch)`: OpenGothic does not preserve
whether the incoming untrigger originated from an untrigger vs. an untouch message
(both map to `T_Untrigger`), so the strictest faithful approximation is to drop the
event only when *both* react flags are off — i.e. forward whenever the original
would forward via *either* `OnUntrigger` or `OnUntouch`. This avoids regressing
touch-only and trigger-only triggers while still suppressing the case the original
always suppresses (both react flags false).
