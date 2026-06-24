# on_state[] fires once per command, not once per state reached (multi-state items)

**Confidence:** Medium-High (divergence proven against the binary; faithful fix is cross-subsystem, so the patch is DEFERRED).

## Original function + address (prose only)

The item special-use state machine in `Gothic2.exe` lives in `oCNpc::EV_UseItemToState`
(0x007558f0), driven through `oCNpc::EV_UseItem` (0x00755620) for the simple-use case and
`oCItem::GetStateEffectFunc` (0x00712b80) for the per-state script lookup.

- `oCItem::GetStateEffectFunc(state)` returns the item's `on_state[state]` script function index
  only when `state` is in `[0,3]` and `on_state[state] >= 1`; otherwise it returns -1.

- `oCNpc::EV_UseItem` first determines the item's highest valid use state by probing animation
  names `S0`, `S1`, ... via `zCModel::GetAniIDFromAniName` until the lookup fails, stores that
  highest index as the message's target state, and then delegates to `EV_UseItemToState`. So even a
  plain "use" (a potion, a food, a multi-state instrument such as a lute) does not jump straight to
  a single state -- it walks every state from 0 up to the highest available one.

- `oCNpc::EV_UseItemToState` is a per-tick state machine. It keeps a current state
  (`this+0x96c`) and a target state (`this+0x970`). Each time the transition animation for the
  current state has finished arriving (gated by `oCAniCtrl_Human::IsStanding` / `zCModel::IsAniActive`
  on the current state's `S<n>` animation) and a per-message "already-fired" flag (`param_1+0x70`)
  is still 0, it sets the `SELF` and `ITEM` parser instances, calls
  `GetStateEffectFunc(currentState)`, invokes that `on_state` function via `zCParser::CallFunc`,
  sets the "already-fired" flag, and only then starts the transition animation toward the next
  state and advances `currentState`. The result: `on_state[i]` is invoked exactly once per state
  **actually reached**, in ascending order, at the moment the animation arrives at state `i`.

Net behavior in the original: using a multi-state item fires `on_state[0]`, then `on_state[1]`,
then `on_state[2]`, ... up to the target, each gated on the animation reaching that state.

## OpenGothic file:line

- `game/game/inventory.cpp:795` -- `Inventory::putState` (the `AI_UseItemToState` path).
- `game/game/inventory.cpp:880` / `:962-966` -- `Inventory::use` (the direct / `AI_UseItem` path).
- `game/graphics/mesh/pose.cpp:476-512` -- `Pose::solveNext` advances `itemUseSt` toward
  `itemUseDestSt` one state per finished animation, but with **no per-state script callback**.

## Divergence

OpenGothic fires `on_state[]` synchronously at the moment the use/putState command is issued, and
fires it for exactly one state:

- `Inventory::use` fires only `on_state[0]` (and immediately, before the animation runs), then
  hands the animation to `Pose::setAnimItem` with `state=-1`.
- `Inventory::putState` fires only `on_state[targetState]` (and immediately), then hands the
  animation to the pose machine with `state=targetState`.

The pose machine (`Pose::solveNext`) then silently steps `itemUseSt` from 0 to the target without
calling any script. Consequently:

1. **Count is wrong.** For a multi-state item that defines `on_state[0..N]`, OpenGothic runs at
   most one of those functions per use; the intermediate `on_state[1..N-1]` (and, on the `use`
   path, every state above 0) are dead. The original runs each one.
2. **Timing/order is wrong.** OpenGothic runs the single `on_state` up-front at command-issue,
   decoupled from animation arrival; the original runs each `on_state[i]` only when the player's
   animation has actually reached state `i`, in ascending order.

This affects scripted multi-state instruments/devices (e.g. a lute or a lever-like item with
`S0/S1/S2` use states whose `on_state[]` slots each carry a script) and any item whose
`on_state[1+]` is expected to fire during a normal use walk.

## Proposed patch

DEFERRED.

Reason: a faithful fix is cross-subsystem and not safely expressible as a local edit. The
animation-arrival gating lives in the pose state machine (`Pose::solveNext`,
`game/graphics/mesh/pose.cpp:476-512`), which advances `itemUseSt`/`itemUseDestSt` but exposes no
callback and does not know about the item handle or the script VM. To match the original, each
crossed state transition in `solveNext` would need to invoke
`GameScript::invokeItem(npc, on_state[reachedState])` for the freshly-reached state (with `SELF`
and `ITEM` set), and the up-front firing in `Inventory::use`/`Inventory::putState` would need to be
removed so on_state is no longer fired at command-issue. That requires threading the owning Npc, the
active state item, and the `on_state[]` array into the pose layer, plus serialization of the
per-state "already-fired" flag to keep save/load deterministic. The existing
`Inventory::putState` NOTE (inventory.cpp:805) already documents that only the target-state script
fires today; this finding extends it to the full per-state walk and explains why the surgical local
patch is not available.

Grep-verified symbols referenced: `zenkit::IItem::on_state` (`state_count = 4`, daedalus.hh:293),
`Inventory::putState` / `Inventory::use` (inventory.cpp), `GameScript::invokeItem`
(gamescript.h:136), `Pose::itemUseSt` / `Pose::itemUseDestSt` (pose.h:137-138),
`Pose::solveNext` (pose.cpp:476).
