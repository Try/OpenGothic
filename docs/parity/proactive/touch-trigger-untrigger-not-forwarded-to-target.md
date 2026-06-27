# zCTrigger (plain Trigger): untrigger/untouch is never forwarded to the target

**Confidence:** High (that the divergence exists). Medium-High (on the exact patch — it is
the literal mirror of the already-present `Trigger::onTrigger` forwarder and of the accepted
`TriggerList::onUntrigger` fix).

## Original function + address (prose only)

A plain `zCTrigger` relays events to its `triggerTarget` through two symmetric, thin vtable
entry points, both of which resolve the target-by-name and dispatch to the target's event
manager:

- `zCTrigger::TriggerTarget` (Gothic2.exe @ 0x00610340) — when the trigger's target-name
  field is non-empty, it walks the resolved target vob(s) and invokes their event-manager
  "OnTrigger" slot (EM vtable +0x10). This is the *on* half.
- `zCTrigger::UntriggerTarget` (Gothic2.exe @ 0x006103f0) — same structure (guarded by the
  same non-empty target-name check), but invokes the target event-manager's "OnUntrigger"
  slot (EM vtable +0x14). This is the *off* half.

The *off* half is reached from two places, both of which forward to `UntriggerTarget`
(vtable +0x74):

- `zCTrigger::OnUntrigger` (Gothic2.exe @ 0x00610600): if `reactToOnTrigger` (0x134 & 1),
  not on a timer, enabled (0x135 & 2) and `sendUntrigger` (0x135 & 4) → `UntriggerTarget`.
- `zCTrigger::OnUntouch` (Gothic2.exe @ 0x00610660): identical, but gated on
  `reactToOnTouch` (0x134 & 2) instead.

So once an untrigger/untouch passes the react/enabled/send-untrigger gate, the original
*always forwards an OnUntrigger to the trigger's target*, exactly mirroring how an
OnTrigger/OnTouch is forwarded by `TriggerTarget`.

## OpenGothic file:line

- `game/world/triggers/trigger.cpp:13` — `Trigger::onTrigger` forwards `T_Trigger` to the
  target via `world.triggerEvent`.
- `game/world/triggers/trigger.h:9` — `Trigger` declares **only** `onTrigger`; there is no
  `onUntrigger` override.
- `game/world/triggers/abstracttrigger.cpp:160` — `AbstractTrigger::onUntrigger` is an empty
  body.
- `game/world/triggers/abstracttrigger.cpp:129-139` — `implProcessEvent` already reproduces
  the original's untrigger gate (`disabled`, `sendUntrigger`, and the
  `reactToOnTrigger/reactToOnTouch` react gate) and then calls `onUntrigger(evt)`.

## Divergence

The on-half is forwarded; the off-half is dropped. A plain `zCTrigger` that has passed the
untrigger gate calls the empty `AbstractTrigger::onUntrigger`, so the `OnUntrigger` is never
relayed to its `triggerTarget`. The original forwards it via `UntriggerTarget`.

Observable effect: any target reached *through* a plain trigger gets the "on" event but never
the matching "off" event. Concretely, a setup `SourceTrigger → (plain) Trigger → Mover` (a
door/platform whose target chains through a relay trigger) opens but never closes, because the
untrigger dies at the relay `Trigger`. The same applies to chains of plain triggers — the
untrigger stops at the first one. This is the exact same class of bug already fixed for
`zCTriggerList` (`docs/parity/proactive/trglist2-untrigger-not-forwarded.md`) and gated by the
already-fixed react gate (`docs/parity/proactive/trig-untrigger-react-gate.md`); the plain
`zCTrigger` case is the remaining gap. The forwarding itself is distinct from both prior fixes
(this is the relay, not the gate).

## Proposed patch

Add the missing `onUntrigger` override to the plain `Trigger`, as the literal mirror of the
existing `onTrigger` forwarder. `target` and `vobName` are already used by `Trigger::onTrigger`
(both are protected members of `AbstractTrigger`, `abstracttrigger.h:106-107`), and
`world.triggerEvent` already routes `T_Untrigger` to every trigger named `target`
(`WorldObjects::execTriggerEvent`, `worldobjects.cpp:406` → `processEvent`).

`game/world/triggers/trigger.h`

OLD:
```cpp
class Trigger : public AbstractTrigger {
  public:
    Trigger(Vob* parent, World& world, const zenkit::VirtualObject& data, Flags flags);

    void onTrigger(const TriggerEvent& evt) override;
  };
```
NEW:
```cpp
class Trigger : public AbstractTrigger {
  public:
    Trigger(Vob* parent, World& world, const zenkit::VirtualObject& data, Flags flags);

    void onTrigger(const TriggerEvent& evt) override;
    void onUntrigger(const TriggerEvent& evt) override;
  };
```

`game/world/triggers/trigger.cpp`

OLD:
```cpp
void Trigger::onTrigger(const TriggerEvent&) {
  TriggerEvent e(target,vobName,TriggerEvent::T_Trigger);
  world.triggerEvent(e);
  }
```
NEW:
```cpp
void Trigger::onTrigger(const TriggerEvent&) {
  TriggerEvent e(target,vobName,TriggerEvent::T_Trigger);
  world.triggerEvent(e);
  }

void Trigger::onUntrigger(const TriggerEvent&) {
  // NOTE: in original-game zCTrigger::UntriggerTarget @0x006103f0 a plain trigger relays an
  // OnUntrigger to its triggerTarget exactly as zCTrigger::TriggerTarget @0x00610340 relays
  // OnTrigger (reached from OnUntrigger @0x00610600 / OnUntouch @0x00610660 after the
  // react/enabled/send-untrigger gate, which OpenGothic already reproduces in
  // AbstractTrigger::implProcessEvent). OpenGothic overrode only onTrigger, so the gated
  // untrigger hit the empty AbstractTrigger::onUntrigger and was dropped -- targets reached
  // through a plain trigger got the "on" half but never the "off" half.
  TriggerEvent e(target,vobName,TriggerEvent::T_Untrigger);
  world.triggerEvent(e);
  }
```
