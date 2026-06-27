# Enable/Disable/Toggle messages incorrectly routed through the activation fire-delay gate

**Confidence:** High

## Original function + address
In the original `Gothic2.exe`, a `zCMessageFilter` translates its trigger/untrigger
into one of `MT_NONE / MT_TRIGGER / MT_UNTRIGGER / MT_ENABLE / MT_DISABLE /
MT_TOGGLE_ENABLED` and dispatches it in `zCMessageFilter::ProcessMessage`
(`0x00618620`, reached from `zCMessageFilter::OnTrigger` `0x00618930` and
`OnUntrigger` `0x00618950`). The crucial detail is the *delivery path per message
type*:

- `MT_TRIGGER` / `MT_UNTRIGGER` are delivered through the target vob's
  `OnTrigger` / `OnUntrigger` virtual (EventManager vtable slots `+0x10` / `+0x14`).
- `MT_ENABLE` / `MT_DISABLE` / `MT_TOGGLE_ENABLED` are delivered as a
  `zCEventCommon` object (subtype 0/1/2) through the *separate* `OnMessage`
  virtual (EventManager vtable slot `+0x28`).

For a `zCTrigger`, `zCTrigger::OnMessage` (`0x006106e0`) handles the enable/disable
event by flipping the enabled bit (`this[0x135] & 2`) **immediately and
unconditionally**: subtype 0 sets the bit, 1 clears it, 2 toggles it. It never
touches the fire-delay, retrigger-delay, activation-count, pending-event, or
react flags. All of that activation gating lives only on the `OnTrigger` path
(`zCTrigger::OnTrigger 0x006105e0` -> `ActivateTrigger 0x006104d0` ->
`CanBeActivatedNow 0x00610220`, which is what reads retrigger-delay / on-timer /
last-trigger-time). Enable/disable are intentionally outside that gate.

## OpenGothic file:line
- `game/world/triggers/abstracttrigger.cpp:73` `AbstractTrigger::processEvent`
- `game/world/triggers/abstracttrigger.cpp:91` `AbstractTrigger::implProcessEvent`
  (cases `T_Enable` / `T_Disable` / `T_ToggleEnable` at lines 121-129)
- emitted from `game/world/triggers/messagefilter.cpp:34-48`

## Divergence
In OpenGothic every event type, including `T_Enable` / `T_Disable` /
`T_ToggleEnable`, is funneled through `AbstractTrigger::processEvent`, which
applies the activation gating *before* the type is even examined:

```
if(hasDelayedEvents()) return;                                   // dropped
if(0!=emitTimeLast && tick < emitTimeLast+retriggerDelay) return;// dropped
if(fireDelay>0) { defer by fireDelay; return; }                  // delayed
implProcessEvent(evt); // only here is the type switched and `disabled` set
```

Consequently, when a `MessageFilter` (or any sender) enables/disables a target
`zCTrigger` that has a `fire_delay`, a `retrigger_delay`, or a currently pending
delayed fire, the enable/disable is silently dropped or postponed — whereas the
original applies it instantly via `OnMessage`. Additionally `implProcessEvent`
sets `emitTimeLast = tickCount()` at its top for *every* type, so even a
successfully-applied enable/disable poisons the retrigger window of the target,
suppressing a legitimate later `T_Trigger` — something `zCTrigger::OnMessage`
never does. This is a message-type routing bug: control messages must bypass the
OnTrigger activation gate entirely.

## Proposed patch
Handle the enable/disable/toggle control messages immediately in
`processEvent`, before the activation gates, and without going through
`implProcessEvent` (so `emitTimeLast` is left untouched, matching
`zCTrigger::OnMessage`). The fields `disabled`, `fireDelay`, `retriggerDelay`,
`emitTimeLast`, `delayedEvent` are all grep-verified members of
`AbstractTrigger` (`abstracttrigger.h:88-102`).

OLD (`game/world/triggers/abstracttrigger.cpp:73`):
```cpp
void AbstractTrigger::processEvent(const TriggerEvent& evt) {
  if(hasDelayedEvents()) {
    // discard, if already have pending
    return;
    }
```

NEW:
```cpp
void AbstractTrigger::processEvent(const TriggerEvent& evt) {
  // NOTE: in original-game zCTrigger::OnMessage @0x006106e0 the enable/disable/toggle
  // messages arrive as a zCEventCommon via the OnMessage vtable slot, separate from the
  // OnTrigger path (ActivateTrigger @0x006104d0 / CanBeActivatedNow @0x00610220). They flip
  // the enabled flag immediately, bypassing fire-delay/retrigger-delay/pending-event gating
  // and without updating the last-trigger timestamp. Mirror that here.
  switch(evt.type) {
    case TriggerEvent::T_Enable:
      disabled = false;
      return;
    case TriggerEvent::T_Disable:
      disabled = true;
      return;
    case TriggerEvent::T_ToggleEnable:
      disabled = !disabled;
      return;
    default:
      break;
    }
  if(hasDelayedEvents()) {
    // discard, if already have pending
    return;
    }
```

(The corresponding `T_Enable` / `T_Disable` / `T_ToggleEnable` cases in
`implProcessEvent` become dead for the trigger path but can be left in place,
since `implProcessEvent` is still reachable via `processDelayedEvents` for the
fire-delayed `T_Trigger`/`T_Touch` events only.)
