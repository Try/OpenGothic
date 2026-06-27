# Consumable on_state effect runs without the `item` global instance bound

**Confidence:** Medium (divergence proven against the binary; faithful, surgical, build-verifiable fix. False-positive risk: depends on whether a given `on_state` script reads the `item` global rather than `self` — most stock G2 food/potion `Use_*` functions touch only `self`, but the engine guarantee exists and mods/scripts rely on it.)

## Original function + address (prose only)

When the original applies a consumable's per-state effect it runs the `on_state[i]`
script with **two** parser instances bound: `self` (the consuming NPC) and `item`
(the item being used).

- `oCNpc::EV_UseItemToState` (Gothic2.exe `0x007558f0`) is the per-tick state machine
  that drives food/potion/multi-state item use (reached from the `oCMsgManipulate`
  type-5 `EV_UseItem` path at `0x00755620`, dispatched in `oCNpc::OnMessage`
  `0x0074c6bf`). Immediately before firing the per-state effect it executes, in order:
  `zCParser::SetInstance("SELF", this)`, then `zCParser::SetInstance("ITEM", interactItem)`
  (the item held in `this+0x968`), then looks up the function via
  `oCItem::GetStateEffectFunc(interactItem, currentState)` (`0x00712b80`, returns
  `on_state[state]` for state in 0..3 when the slot value is > 0) and invokes it with
  `zCParser::CallFunc`. A one-shot flag (`param_1+0x70`) fires it once per state arrival.
  So **the `on_state` callback always sees `item` == the used item.**

- By contrast the equip-effect path `oCNpc::AddItemEffects` (`0x007320f0`) binds **only**
  `SetInstance("SELF", this)` before calling the item's `on_equip` function (field at
  `+0x1e8`); it does NOT bind `item`. `oCNpc::RemoveItemEffects` (`0x00732270`) mirrors this
  for `on_unequip`. So the asymmetry is deliberate: `on_equip`/`on_unequip` run with only
  `self`; `on_state` runs with `self` AND `item`.

(Side note, verified while investigating: the separate `oCMsgUseItem` → `oCNpc::EV_Drink`
`0x007531c0` "T_DRINKPOTION" path — which applies the effect at 65 % animation progress via
`oCNpc::UseItem` `0x0073bc10` — is **dead code in Gothic2**: the `oCMsgUseItem` constructors
`0x00767a20`/`0x00767b10` have no in-binary xrefs, so no message of that class is ever posted.
The live food/potion path is `EV_UseItem`/`EV_UseItemToState`. The pure *timing* divergence —
OpenGothic firing `on_state[0]` immediately at animation start instead of at the swallow frame —
is already captured and DEFERRED in `onstate-multistate-walk-count.md`; this finding is the
distinct, surgical instance-binding gap on the same code path.)

## OpenGothic file:line

- `game/game/inventory.cpp:980-983` — `Inventory::use()` fires `on_state[0]` via
  `GameScript::invokeItem` (`game/game/gamescript.cpp:1131`).
- `game/game/inventory.cpp:824-826` — `Inventory::putState()` fires `on_state[state]` via the
  same `invokeItem`.

`GameScript::invokeItem` binds **only** `self`:

```cpp
ScopeVar self(*vm.global_self(), npc->handlePtr());
vm.call_function<void>(functionSymbol);
```

It never touches `vm.global_item()`. Neither call site sets the item instance beforehand
(`setCurrentItem(it->clsId())` only updates the inventory-side `curItem` field, not the
script global). So in OpenGothic a consumable's `on_state` effect runs with `item` left at
whatever it was from the previous script call — stale, not the consumed item.

## Divergence

Original: `on_state[i]` runs with `item` bound to the used item (and `self` to the NPC).
OpenGothic: `on_state[i]` runs with `self` bound but `item` stale. Any `on_state` script that
references the `item` global (e.g. to read/transform/destroy the consumed instance) observes the
wrong instance. The `on_equip`/`on_unequip` paths are already faithful (original binds only
`self` there too), so the fix must be scoped to the two `on_state` call sites, not to
`invokeItem` itself.

## Proposed patch

Grep-verified symbol `GameScript::storeItem(Item*)` (`game/game/gamescript.cpp:830`,
declared `game/game/gamescript.h:222`) already sets `vm.global_item()` to an item handle —
the exact `SetInstance("ITEM", ...)` mirror. `it` is in scope at both call sites
(`Item* it` from `findByClass`).

`game/game/inventory.cpp` — `Inventory::use()`:

```cpp
// OLD
  setCurrentItem(it->clsId());
  if(itData.on_state[0]!=0){
    auto& vm = owner.world().script();
    vm.invokeItem(&owner,uint32_t(itData.on_state[0]));
    }
```
```cpp
// NEW
  setCurrentItem(it->clsId());
  if(itData.on_state[0]!=0){
    auto& vm = owner.world().script();
    // NOTE: in original-game oCNpc::EV_UseItemToState @0x007558f0 binds the parser ITEM
    // instance to the used item (SetInstance("ITEM",interactItem)) before invoking
    // on_state[state]; the on_equip path (AddItemEffects @0x007320f0) binds only SELF, so
    // the item binding is specific to on_state. Mirror it here.
    vm.storeItem(it);
    vm.invokeItem(&owner,uint32_t(itData.on_state[0]));
    }
```

`game/game/inventory.cpp` — `Inventory::putState()`:

```cpp
// OLD
  if(state>=0 && state<4 && it->handle().on_state[size_t(state)]!=0) {
    owner.world().script().invokeItem(&owner,uint32_t(it->handle().on_state[size_t(state)]));
    }
```
```cpp
// NEW
  if(state>=0 && state<4 && it->handle().on_state[size_t(state)]!=0) {
    // NOTE: in original-game oCNpc::EV_UseItemToState @0x007558f0 binds the parser ITEM
    // instance to the used item before invoking on_state[reached_state].
    owner.world().script().storeItem(it);
    owner.world().script().invokeItem(&owner,uint32_t(it->handle().on_state[size_t(state)]));
    }
```

Faithful to the original, which leaves the `item` instance set after the call (no
save/restore); `storeItem` matches that (no scope guard), unlike the equip path that intentionally
leaves `item` unbound.
