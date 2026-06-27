# zCTrigger retrigger window is poisoned by untriggers and dropped events

**Confidence:** High

## Original function + address (prose only)

In `Gothic2.exe` the per-trigger "retrigger countdown" lives in `zCTrigger` field
`0x15c` and is the only state the engine consults to enforce `retriggerDelay`
(`CanBeActivatedNow` @0x00610220 rejects activation while that countdown is still
above a small epsilon). This field is written in exactly two places, and both are
the *successful fire* sites:

- `zCTrigger::ActivateTrigger` @0x006104d0 — the immediate (no fire-delay) path.
  After it calls `TriggerTarget` (vtable +0x70) it decrements the activation count
  (field `0x164`) and sets field `0x15c = retriggerDelay_sec * 1000 + eps`.
- `zCTrigger::OnTimer` @0x00610750 — the delayed (fire-delay) path. After the timer
  elapses it likewise calls `TriggerTarget`, decrements `0x164`, and sets
  field `0x15c = retriggerDelay_sec * 1000 + eps`.

Crucially, the untrigger path never touches this countdown: `zCTrigger::OnUntrigger`
@0x00610600 and `zCTrigger::OnUntouch` @0x00610660 only check `IsOnTimer`, the
enabled flag and `sendUntrigger`, then relay through `UntriggerTarget` @0x006103f0
(vtable +0x74). They do not write field `0x15c`, do not write `0x164`, and do not
record any "last emit" timestamp. Likewise, an event that fails `CanBeActivatedNow`
(disabled, activation count exhausted, wrong responder/name filter) leaves field
`0x15c` untouched. So in the original the retrigger window is measured strictly from
the last *successful trigger fire*.

## OpenGothic file:line

`game/world/triggers/abstracttrigger.cpp:112` (inside
`AbstractTrigger::implProcessEvent`), with the gate it feeds at line 98.

## Divergence

OpenGothic stamps `emitTimeLast = world.tickCount();` as the very first statement of
`implProcessEvent`, before the event type is even examined. That function is the
common sink for *every* delivered event: triggers, touches, and untriggers, as well
as trigger events that are immediately dropped a few lines later (`disabled`,
`emitCount>=maxActivationCount`, react-flag off). The retrigger gate at line 98
(`if(0!=emitTimeLast && world.tickCount()<emitTimeLast+retriggerDelay) return;`) then
measures `retriggerDelay` from that stamp.

Consequences when `retriggerDelay>0`:

- An `OnUntrigger`/`OnUntouch` (e.g. an NPC leaving a touch box, or an upstream
  trigger releasing) resets the window to the moment of *release*. A subsequent
  legitimate trigger that arrives within `retriggerDelay` of the release is then
  suppressed, even though the original measures the window from the earlier *fire*
  and would allow it. Because a release is always later than the fire that preceded
  it, OpenGothic is strictly more restrictive than `Gothic2.exe` here.
- An event that is dropped while `disabled` or after the activation count is
  exhausted still advances `emitTimeLast`, so the first valid trigger after the
  trigger is re-enabled can be wrongly swallowed by the retrigger gate.

The original only ever moves the window forward on a real `TriggerTarget`, so neither
of these happens.

## Proposed patch

Set `emitTimeLast` at the actual fire site (immediately before the activation
counts up and `onTrigger` runs), instead of unconditionally at function entry. This
mirrors `ActivateTrigger`/`OnTimer` writing field `0x15c` only after `TriggerTarget`,
and leaves the window untouched on untriggers and on dropped events. Both the
immediate and fire-delayed paths funnel their real fire through this same branch
(the delayed path re-enters via `processDelayedEvents`), so both keep stamping the
window exactly as the original's two write sites do.

OLD (`game/world/triggers/abstracttrigger.cpp`):

```cpp
void AbstractTrigger::implProcessEvent(const TriggerEvent& evt) {
  emitTimeLast = world.tickCount();
  switch(evt.type) {
    case TriggerEvent::T_Startup:
    case TriggerEvent::T_StartupFirstTime:
    case TriggerEvent::T_Trigger:
    case TriggerEvent::T_Touch:
      if(!reactToOnTouch && evt.type==TriggerEvent::T_Touch)
        return;
      if(!reactToOnTrigger && evt.type==TriggerEvent::T_Trigger)
        return;
      if(disabled)
        return;
      if(emitCount>=maxActivationCount)
        return;
      ++emitCount;
      onTrigger(evt);
      break;
```

NEW:

```cpp
void AbstractTrigger::implProcessEvent(const TriggerEvent& evt) {
  switch(evt.type) {
    case TriggerEvent::T_Startup:
    case TriggerEvent::T_StartupFirstTime:
    case TriggerEvent::T_Trigger:
    case TriggerEvent::T_Touch:
      if(!reactToOnTouch && evt.type==TriggerEvent::T_Touch)
        return;
      if(!reactToOnTrigger && evt.type==TriggerEvent::T_Trigger)
        return;
      if(disabled)
        return;
      if(emitCount>=maxActivationCount)
        return;
      // NOTE: in original-game zCTrigger the retrigger countdown (field_0x15c) is written only
      // after a successful TriggerTarget, in zCTrigger::ActivateTrigger @0x006104d0 (immediate)
      // and zCTrigger::OnTimer @0x00610750 (fire-delayed). zCTrigger::OnUntrigger @0x00610600 /
      // OnUntouch @0x00610660 and any event that fails CanBeActivatedNow @0x00610220 never touch
      // it, so the retrigger window is measured from the last real fire. Stamp emitTimeLast here
      // (the fire site) rather than at function entry, so untriggers and dropped events no longer
      // shrink the retrigger window relative to the original.
      emitTimeLast = world.tickCount();
      ++emitCount;
      onTrigger(evt);
      break;
```

Grep-verified symbols: `emitTimeLast` (abstracttrigger.h:100, set/used at abstracttrigger.cpp:98/112/195/205),
`implProcessEvent`, `emitCount`, `maxActivationCount`, `retriggerDelay`, `world.tickCount()`
all exist in `game/world/triggers/abstracttrigger.{h,cpp}`.
