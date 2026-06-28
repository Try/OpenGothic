# Trigger untrigger relay is gated by retrigger-wait and fire-delay, but in the original it bypasses both

**Confidence:** High

## Original fn + address
In `Gothic2.exe` the OnTrigger/OnTouch activation path and the OnUntrigger/OnUntouch
relay path are *separate vtable slots* with different gating.

- `zCTrigger::OnTrigger` @0x006105e0 and `zCTrigger::OnTouch` @0x00610640 both funnel into
  `zCTrigger::ActivateTrigger` @0x006104d0, which calls `zCTrigger::CanBeActivatedNow`
  @0x00610220 (checks the retrigger-wait timestamp `field_0x15c`, the activation count
  `field_0x164`, `IsOnTimer`, and the enabled flag) and then applies the fire-delay
  `field_0x158`: if `fireDelaySec > 0` it arms a `SetOnTimer` and fires later via
  `zCTrigger::OnTimer` @0x00610750.
- `zCTrigger::OnUntrigger` @0x00610600 and `zCTrigger::OnUntouch` @0x00610660 do **not**
  touch `CanBeActivatedNow`, `field_0x15c`, the count, or `field_0x158`. Their *entire*
  gate is: `IsOnTimer() == 0` (no fire-delay timer currently pending) **and** enabled
  (`field_0x135 & 2`) **and** sendUntrigger (`field_0x135 & 4`); if so they immediately call
  `UntriggerTarget` (slot 0x74). So an untrigger is relayed even while the trigger is inside
  its retrigger-wait cooldown, and it is relayed *immediately*, never postponed by the
  fire-delay.

## OG file:line
`game/world/triggers/abstracttrigger.cpp:73` — `AbstractTrigger::processEvent`
(retrigger gate at lines 98-101, fire-delay scheduling at lines 102-107).

## Divergence
In OpenGothic every trigger event, including `T_Untrigger`, is dispatched through
`WorldObjects::execTriggerEvent` -> `AbstractTrigger::processEvent`
(`worldobjects.cpp:414`). `processEvent` applies the retrigger-wait gate
(`if(0!=emitTimeLast && world.tickCount()<emitTimeLast+retriggerDelay) return;`) and the
fire-delay scheduling (`if(fireDelay>0){ ... return; }`) to *all* non-enable/disable event
types. So for a `zCTrigger` with `retrigger_delay_sec > 0` and/or `fire_delay_sec > 0`:

- An untrigger arriving inside the retrigger cooldown is **dropped** (original relays it),
  potentially leaving the downstream target stuck in its "on" state.
- An untrigger on a fire-delayed trigger is **postponed** by `fireDelay` (original sends it
  immediately).

The `hasDelayedEvents()` early-return at lines 94-97 already matches the original's
`IsOnTimer()==0` drop (an untrigger is suppressed while a fire-delay timer is in flight), so
that check must be preserved for untriggers; only the retrigger gate and the fire-delay
scheduling must be skipped. This is distinct from the already-applied
"stamp `emitTimeLast` at the fire site" fix (that addressed untriggers *poisoning* the
window; this addresses untriggers being *blocked/postponed by* the window and fire-delay).

## Proposed patch
In `AbstractTrigger::processEvent`, route `T_Untrigger` straight to `implProcessEvent` after
the pending-event check, bypassing the retrigger and fire-delay gates:

OLD (`game/world/triggers/abstracttrigger.cpp`, after the enable/disable switch):
```cpp
  if(hasDelayedEvents()) {
    // discard, if already have pending
    return;
    }
  if(0!=emitTimeLast && world.tickCount()<emitTimeLast+retriggerDelay) {
    // need to discard event
    return;
    }
  if(fireDelay>0) {
```

NEW:
```cpp
  if(hasDelayedEvents()) {
    // discard, if already have pending
    return;
    }
  if(evt.type==TriggerEvent::T_Untrigger) {
    // NOTE: in original-game zCTrigger::OnUntrigger @0x00610600 (and OnUntouch @0x00610660)
    // the untrigger relay is a separate vtable path from OnTrigger/OnTouch -> ActivateTrigger
    // @0x006104d0. It never consults CanBeActivatedNow @0x00610220, the retrigger-wait
    // timestamp (field_0x15c) or the fire-delay (field_0x158); its only gates are IsOnTimer
    // (a pending fire-delay timer), the enabled flag and sendUntrigger. OpenGothic funneled
    // untriggers through the shared retrigger/fire-delay gates below, so an untrigger arriving
    // inside the retrigger cooldown was dropped and an untrigger on a fire-delayed trigger was
    // postponed. The IsOnTimer drop is preserved by the hasDelayedEvents() check above.
    implProcessEvent(evt);
    return;
    }
  if(0!=emitTimeLast && world.tickCount()<emitTimeLast+retriggerDelay) {
    // need to discard event
    return;
    }
  if(fireDelay>0) {
```

`implProcessEvent`'s existing `T_Untrigger` branch already reproduces the remaining original
gates (`disabled`/enabled, `sendUntrigger`, and the react-flag filter) and correctly leaves
`emitTimeLast`/`emitCount` untouched.
