# zCTriggerScript: SELF/OTHER/ITEM script context not established before script call

**Confidence:** High (on the divergence; partial on the patch — see DEFERRED note on `self` for the direct-NPC-touch case)

## Original function + address

`oCTriggerScript::TriggerTarget(zCVob*)` at **0x0043c4c0** (Gothic2.exe). After
forwarding to the base `zCTrigger::TriggerTarget`, before invoking the script
function it deterministically establishes the Daedalus instance context, in this
order:

1. `parser.SetInstance("SELF",  <param_1 dynamic_cast to oCNpc>)` — SELF becomes the
   vob that activated the trigger, cast to an NPC; this yields **null** whenever the
   activator is not an oCNpc (e.g. the previous trigger in a trigger-chain).
2. `parser.SetInstance("OTHER", null)` — OTHER is always cleared.
3. `parser.SetInstance("ITEM",  null)` — ITEM is always cleared.
4. `parser.CallFunc(scriptFunc)` — the function name stored on the vob (offset
   `this+0x168`, set by `oCTriggerScript::SetScriptFunc` @0x0043cab0).

The parser is **not** restored afterward; the original leaves SELF/OTHER/ITEM at the
values set above. The companion `oCTriggerScript::UntriggerTarget` @0x0043c820 does
**not** call the script at all (it only logs and forwards to the base) — OpenGothic
already matches this, since `TriggerScript` overrides only `onTrigger`.

## OpenGothic file:line

`/Users/admin/Downloads/opengothic/game/world/triggers/triggerscript.cpp:13-20`

```cpp
void TriggerScript::onTrigger(const TriggerEvent &) {
  try {
    world.script().getVm().call_function(function);
    }
  catch(const std::exception& e){
    Tempest::Log::e("exception in trigger-script: ",e.what());
    }
  }
```

## Divergence

OpenGothic invokes the trigger's script function **without establishing any instance
context**. SELF/OTHER/ITEM retain whatever values the *previous, unrelated* script
call left in the VM globals (commonly the player as SELF/OTHER from a perception or
dialog call). The original always overwrites OTHER and ITEM with null and SELF with
the activating vob (null in the dominant trigger-chain case). Consequently a Daedalus
trigger-script function that reads `other`/`item` (or `self`) observes stale instances
in OpenGothic instead of the cleared/activator context the script was authored against.

Note that OpenGothic's touch pipeline (`AbstractTrigger::onIntersect` →
`TriggerEvent(target, vobName, T_Touch)`, abstracttrigger.cpp:174-185) sets the event
`emitter` to the trigger's **own** `vobName` and discards the touching `Npc*`, so the
NPC pointer the original would cast into SELF is not recoverable inside `onTrigger`.

## Proposed patch

Establish the original's context before the call. OTHER and ITEM are cleared
unconditionally (exact match); SELF is set to null, which matches the original in the
overwhelmingly common trigger-chain activation (non-NPC activator → cast yields null).
Grep-verified symbols: `world.script().getVm()` (already used here),
`vm.global_self()/global_other()/global_item()` (gamescript.cpp:92,528,831),
`DaedalusSymbol::set_instance(...)` (gamescript.cpp:26,31).

OLD:
```cpp
void TriggerScript::onTrigger(const TriggerEvent &) {
  try {
    world.script().getVm().call_function(function);
    }
  catch(const std::exception& e){
    Tempest::Log::e("exception in trigger-script: ",e.what());
    }
  }
```

NEW:
```cpp
void TriggerScript::onTrigger(const TriggerEvent &) {
  try {
    auto& vm = world.script().getVm();
    // NOTE: in original-game oCTriggerScript::TriggerTarget @0x0043c4c0 the Daedalus
    // context is reset before the script call: SELF = activating vob cast to oCNpc
    // (null for non-NPC activators, i.e. the dominant trigger-chain case), OTHER = null,
    // ITEM = null. The original does not restore these afterwards.
    vm.global_self() ->set_instance(nullptr);
    vm.global_other()->set_instance(nullptr);
    vm.global_item() ->set_instance(nullptr);
    vm.call_function(function);
    }
  catch(const std::exception& e){
    Tempest::Log::e("exception in trigger-script: ",e.what());
    }
  }
```

**DEFERRED (partial):** Setting SELF to the *touching NPC* in the rare case where an
`oCTriggerScript` is activated directly by an NPC touch is not implemented, because
OpenGothic's `AbstractTrigger::onIntersect` overwrites the event emitter with the
trigger's own `vobName` and drops the `Npc&` before `onTrigger` runs. Faithfully
reproducing that sub-case requires threading the activating `Npc*` (or the original
activator vob) through `TriggerEvent`/`processEvent` into `onTrigger`, which is a
larger pipeline change and out of scope for this surgical fix. The proposed patch
already matches the original for all non-NPC activations and corrects the stale
OTHER/ITEM leak unconditionally.
