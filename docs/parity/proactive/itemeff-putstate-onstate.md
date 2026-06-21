# Item-state on_state callback never fires on AI_UseItemToState path

**Confidence:** Medium

## Original function + address (prose)
`oCNpc::EV_UseItemToState` (Gothic2.exe `0x007558f0`) drives a multi-state
item-in-hand interaction. As the NPC reaches each animation state, the function
calls `oCItem::GetStateEffectFunc(item, current_state)` (`0x00712b80`) which
returns the item's `on_state[current_state]` script symbol (valid only for
state index 0..3, and only when the slot value is > 0), and then invokes that
script function with `self`/`item` instances bound. A one-shot guard
(`param_1+0x70`) ensures the callback fires exactly once per state arrival.
The destination state itself is computed by `EV_UseItem` (`0x00755620`) by
scanning the item scheme for the highest existing `S<n>` animation.

The net effect in the original: when a scripted `AI_UseItemToState` /
`AI_UseMob`-with-item interaction moves an item to state N, the item's
`on_state[N]` function runs.

## OpenGothic location
`game/game/inventory.cpp:789` (`Inventory::putState`), reached from
`game/world/objects/npc.cpp:2686` (`case AI_UseItemToState` ->
`invent.putState(*this, itm, state)`).

`putState` calls `owner.setAnimItem(scheme, state)` to drive the pose
state-machine (`game/graphics/mesh/pose.cpp:873`/`476`), but it invokes **no**
`on_state` script function for the reached state. The only `on_state`
invocation in the whole tree is `Inventory::use()` at
`game/game/inventory.cpp:916-918`, and that hardcodes index `[0]`.

## Divergence
- Original: reaching item-state N invokes `on_state[N]`.
- OpenGothic: reaching item-state N via `putState`/`AI_UseItemToState` invokes
  nothing. `on_state[1..3]` are dead, and even `on_state[0]` is not invoked on
  this path (only the separate `use()` path fires `on_state[0]`).

Gameplay-different for multi-state in-hand items whose scripts attach effects
per state (e.g. items with `S0/S1`+ transitions and populated `on_state[]`):
the per-state script logic never runs.

## Proposed patch
```cpp
// game/game/inventory.cpp  (Inventory::putState)
// OLD
  if(!owner.setAnimItem(it->handle().scheme_name,state))
    return false;

  setCurrentItem(0);
  setStateItem(cls);
  return true;
  }
// NEW
  if(!owner.setAnimItem(it->handle().scheme_name,state))
    return false;

  // NOTE: in original-game oCNpc::EV_UseItemToState invokes the item's
  // on_state[reached_state] (via oCItem::GetStateEffectFunc) once per state
  // arrival; reaching state N must run on_state[N].
  if(state>=0 && state<4 && it->handle().on_state[size_t(state)]!=0) {
    auto& vm = owner.world().script();
    vm.invokeItem(&owner,uint32_t(it->handle().on_state[size_t(state)]));
    }

  setCurrentItem(0);
  setStateItem(cls);
  return true;
  }
```

Caveat for the reviewer: this fires `on_state[state]` once at the call site,
which matches the *target-state* semantics of the script API. The original
fires it once per intermediate state crossed by the animation machine; a
fully faithful port would invoke the callback from the pose state-machine as
each state is actually reached. The surgical patch above covers the common
single-target case driven by `AI_UseItemToState`.
