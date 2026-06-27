# zCTriggerList: untrigger is never forwarded to list targets

**Confidence:** High (that the divergence exists); Medium-High (on the exact patch — the
untrigger path shares the list index and per-target delay logic with the trigger path).

## Original function + address (prose only)

`zCTriggerList` routes *both* directions through one private worker,
`zCTriggerList::DoTriggering` (Gothic2.exe @ 0x00615190). There are two thin vtable
entry points:

- `zCTriggerList::TriggerTarget` (@ 0x00615460) sets the internal direction flag at
  object offset +0x200 to `1` and calls `DoTriggering`.
- `zCTriggerList::UntriggerTarget` (@ 0x00615470) sets that same flag at +0x200 to `0`
  and calls `DoTriggering` (guarded by a per-trigger flag bit).

Inside the per-target dispatch `zCTriggerList::TriggerActTarget` (@ 0x00614f30) the +0x200
flag selects which message is sent to each selected target's event manager: when the flag
is `1` it invokes the manager's "OnTrigger" slot, when it is `0` it invokes the "OnUntrigger"
slot. So an untrigger reaching the list is *forwarded as OnUntrigger to the list's targets*,
using the very same ALL/NEXT/RANDOM selection, per-target fire-delay accumulation, and shared
list index (offset +0x1fc) as the trigger path.

The dispatch into `UntriggerTarget` is gated by `zCTrigger::OnUntrigger`
(@ 0x00610600): it requires the trigger enabled, not busy on a timer, the react bit
(0x135 & 2), and the send-untrigger bit (0x135 & 4). OpenGothic already reproduces that
gate upstream in `AbstractTrigger::implProcessEvent` (`disabled`, `sendUntrigger`,
`reactToOnTrigger/reactToOnTouch`) before calling `onUntrigger`.

## OpenGothic file:line

- `game/world/triggers/triggerlist.cpp:16-57` — `TriggerList::onTrigger` is the only event
  handler; it emits `TriggerEvent::T_Trigger` to targets.
- `game/world/triggers/triggerlist.h:13` — only `onTrigger` is overridden; there is no
  `onUntrigger`.
- `game/world/triggers/abstracttrigger.cpp:160-161` — the inherited
  `AbstractTrigger::onUntrigger` is an empty stub.

## Divergence

In OpenGothic a `T_Untrigger` that survives the activation gates lands on the empty
`AbstractTrigger::onUntrigger` and is silently dropped: a `TriggerList` never relays
OnUntrigger to its targets. In the original, the same untrigger runs `DoTriggering` in
untrigger mode and sends OnUntrigger to the selected target(s) exactly as a trigger sends
OnTrigger. Net effect: content that drives a set of targets through a `zCTriggerList`
(e.g. open-on-trigger / close-on-untrigger movers, sound stop, mob deactivation) gets the
"on" half but never the "off" half in OpenGothic.

(Also note, consistent with this: in the original NEXT/RANDOM modes the untrigger advances
the *same* list index +0x1fc as the trigger, because both go through `DoTriggering`. A
faithful fix must therefore share OpenGothic's `next` field across both directions, which
the patch below does by routing both through one helper.)

## Proposed patch

Route both directions through one helper that takes the event type, so the ALL cumulative
delay, NEXT/RANDOM selection, and the shared `next` index are reused verbatim.

`game/world/triggers/triggerlist.h`

OLD:
```cpp
    void onTrigger(const TriggerEvent& evt) override;

  private:
    void save(Serialize &fout) const override;
    void load(Serialize &fin) override;
```
NEW:
```cpp
    void onTrigger(const TriggerEvent& evt) override;
    void onUntrigger(const TriggerEvent& evt) override;

  private:
    void emitList(TriggerEvent::Type type);
    void save(Serialize &fout) const override;
    void load(Serialize &fin) override;
```

`game/world/triggers/triggerlist.cpp`

OLD:
```cpp
void TriggerList::onTrigger(const TriggerEvent&) {
  if(targets.empty())
    return;

  switch(listProcess) {
    case zenkit::TriggerBatchMode::ALL: {
      uint64_t offset = 0;
      for(auto& i:targets) {
        offset += uint64_t(i.delay*1000);
        uint64_t time = world.tickCount()+offset;
        TriggerEvent ex(i.name,vobName,time,TriggerEvent::T_Trigger);
        world.execTriggerEvent(ex);
        }
      break;
      }
    case zenkit::TriggerBatchMode::NEXT: {
      auto& i = targets[next];
      next = (next+1)%uint32_t(targets.size());

      uint64_t time = world.tickCount()+uint64_t(i.delay*1000);
      TriggerEvent ex(i.name,vobName,time,TriggerEvent::T_Trigger);
      world.execTriggerEvent(ex);
      break;
      }
    case zenkit::TriggerBatchMode::RANDOM: {
      // NOTE: in original-game zCTriggerList::DoTriggering (Gothic2.exe 0x00615190) RANDOM mode
      // re-rolls while the new index equals the previously fired one, so the same target is
      // never fired twice in a row. OpenGothic rolled once with no exclusion.
      uint32_t idx = world.script().rand(uint32_t(targets.size()));
      if(targets.size()>1)
        while(idx==next)
          idx = world.script().rand(uint32_t(targets.size()));
      next = idx;
      auto& i = targets[idx];

      uint64_t time = world.tickCount()+uint64_t(i.delay*1000);
      TriggerEvent ex(i.name,vobName,time,TriggerEvent::T_Trigger);
      world.execTriggerEvent(ex);
      break;
      }
    }
  }
```
NEW:
```cpp
void TriggerList::onTrigger(const TriggerEvent&) {
  emitList(TriggerEvent::T_Trigger);
  }

void TriggerList::onUntrigger(const TriggerEvent&) {
  // NOTE: in original-game zCTriggerList::UntriggerTarget @0x00615470 an untrigger runs the
  // same zCTriggerList::DoTriggering @0x00615190 worker with the +0x200 direction flag cleared,
  // so TriggerActTarget @0x00614f30 relays OnUntrigger to the selected target(s) using the same
  // ALL/NEXT/RANDOM selection, per-target fire-delay, and shared list index as the trigger path.
  // OpenGothic dropped untriggers on the empty AbstractTrigger::onUntrigger.
  emitList(TriggerEvent::T_Untrigger);
  }

void TriggerList::emitList(TriggerEvent::Type type) {
  if(targets.empty())
    return;

  switch(listProcess) {
    case zenkit::TriggerBatchMode::ALL: {
      uint64_t offset = 0;
      for(auto& i:targets) {
        offset += uint64_t(i.delay*1000);
        uint64_t time = world.tickCount()+offset;
        TriggerEvent ex(i.name,vobName,time,type);
        world.execTriggerEvent(ex);
        }
      break;
      }
    case zenkit::TriggerBatchMode::NEXT: {
      auto& i = targets[next];
      next = (next+1)%uint32_t(targets.size());

      uint64_t time = world.tickCount()+uint64_t(i.delay*1000);
      TriggerEvent ex(i.name,vobName,time,type);
      world.execTriggerEvent(ex);
      break;
      }
    case zenkit::TriggerBatchMode::RANDOM: {
      // NOTE: in original-game zCTriggerList::DoTriggering (Gothic2.exe 0x00615190) RANDOM mode
      // re-rolls while the new index equals the previously fired one, so the same target is
      // never fired twice in a row. OpenGothic rolled once with no exclusion.
      uint32_t idx = world.script().rand(uint32_t(targets.size()));
      if(targets.size()>1)
        while(idx==next)
          idx = world.script().rand(uint32_t(targets.size()));
      next = idx;
      auto& i = targets[idx];

      uint64_t time = world.tickCount()+uint64_t(i.delay*1000);
      TriggerEvent ex(i.name,vobName,time,type);
      world.execTriggerEvent(ex);
      break;
      }
    }
  }
```

Symbols grep-verified to exist: `TriggerEvent::Type` / `T_Untrigger`
(`abstracttrigger.h:16-18`), the 4-arg `TriggerEvent(target,emitter,t,type)` ctor
(`abstracttrigger.h:30`), `AbstractTrigger::onUntrigger` virtual
(`abstracttrigger.h:71`), `world.execTriggerEvent`, `world.tickCount()`, and the
`targets` / `next` / `listProcess` / `vobName` members used unchanged.
