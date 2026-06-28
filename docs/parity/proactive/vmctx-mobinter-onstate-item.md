# vmctx: mob-interactive on_state func leaves a stale ITEM Daedalus instance

**Confidence:** Medium

## Original fn + address

`oCMobInter::CallOnStateFunc` (Gothic2.exe `0x00720870`) is the engine routine that
fires a mob's `on_state` script (`<onStateFunc>_S<state>`) when an NPC drives the
mobsi through its states. Before it calls the script function it binds **two** parser
globals, in this order:

1. `zCParser::SetInstance(parser, SELF, param_1)` — `SELF` = the interacting NPC.
2. `zCParser::SetInstance(parser, ITEM, *(param_1 + 0x968))` — `ITEM` = the NPC's
   *interact item* (the field at oCNpc offset `0x968`, the same slot managed by
   `oCNpc::SetInteractItem` @`0x0074acc0`, `EV_CreateInteractItem` @`0x00754890`,
   `EV_DestroyInteractItem` @`0x00754b40`). For scheme mobsis with no created
   interact item this is null, so the engine effectively *clears* `ITEM` on every
   `on_state` invocation.

It does **not** bind `OTHER` (consistent with OpenGothic).

This is the same engine idiom OpenGothic already mirrored for the *item* `on_state`
path: `oCNpc::EV_UseItemToState` (`0x007558f0`) binds `ITEM` to the used item before
invoking `on_state[state]` (see the existing NOTEs in `inventory.cpp:836` and
`inventory.cpp:1040`). The `on_equip` path (`AddItemEffects` @`0x007320f0`) binds only
`SELF`. So binding `ITEM` is specific to the *state* call paths — and the mob path is
one of them.

## OG file:line

- `game/world/objects/interactive.cpp:555` — `Interactive::invokeStateFunc`
- `game/game/gamescript.cpp:1410` — `GameScript::useInteractive` (the only callee of
  `invokeStateFunc`), which binds **only** `SELF`:

```
ScopeVar self(*vm.global_self(),hnpc);
vm.call_function<void>(fn);
```

## Divergence

For a mob `on_state` script, the original binds both `SELF` and `ITEM` (the latter to
the NPC's interact item, which is null for the common scheme-mob case). OpenGothic's
`useInteractive` binds only `SELF` and never touches the `ITEM` global, so whatever
instance was last written to `ITEM` by a *previous* script call (e.g. a preceding
`on_state`/`on_equip`/use invocation) remains bound. A mob `on_state` function that
reads `ITEM` therefore sees a stale, unrelated item instead of the engine-cleared
(null) value the original guarantees.

The `ITEM`-not-modeled caveat: OpenGothic does not model the NPC's interact item
(oCNpc `0x968`), so the non-null case (mobs that create an interact item) cannot be
reproduced 1:1. The high-confidence part is that the original *always rebinds* `ITEM`
here while OG never does, leaving it stale; clearing it restores parity for the
overwhelmingly common null-interact-item case and removes the cross-call leak.

## Proposed patch

In `GameScript::useInteractive` (`game/game/gamescript.cpp:1410`), bind `ITEM`
alongside `SELF` so the mob `on_state` script can never observe a stale item, matching
the original's unconditional `SetInstance(ITEM, ...)`.

OLD:
```cpp
void GameScript::useInteractive(const std::shared_ptr<zenkit::INpc>& hnpc, std::string_view func) {
  auto fn = vm.find_symbol_by_name(func);
  if(fn == nullptr)
    return;

  ScopeVar self(*vm.global_self(),hnpc);
```

NEW:
```cpp
void GameScript::useInteractive(const std::shared_ptr<zenkit::INpc>& hnpc, std::string_view func) {
  auto fn = vm.find_symbol_by_name(func);
  if(fn == nullptr)
    return;

  // NOTE: in original-game oCMobInter::CallOnStateFunc (Gothic2.exe 0x00720870) the mob on_state
  // script is invoked with SELF=npc AND ITEM=npc->interactItem (oCNpc 0x968), the latter null for
  // scheme mobsis -- so the engine clears ITEM on every on_state call. OpenGothic bound only SELF,
  // leaving a stale ITEM instance from a prior script call visible to the on_state func. Clear it
  // (interactItem itself is not modelled, so the null/common case is the reproducible parity).
  ScopeVar self(*vm.global_self(), hnpc);
  ScopeVar item(*vm.global_item(), std::shared_ptr<zenkit::IItem>(nullptr));
```

(`ScopeVar` restores the previous `ITEM` on scope exit; the next script call rebinds it
regardless, so the transient restore is parity-neutral.)
