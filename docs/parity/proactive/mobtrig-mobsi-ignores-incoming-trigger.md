# A MOBSI never reacts to an incoming trigger (Wld_SendTrigger / trigger-chain to a mob is a no-op)

**Confidence:** High that the divergence exists and is structural (certain from
decompile + grep). Patch **DEFERRED** — a faithful fix is not a one-liner and the
state-flip half it needs must integrate with the already-deferred mob state-index
machine.

## Original function + address (prose only)

In the original engine `oCMobInter` overrides the two `zCVob` trigger virtuals:

- `oCMobInter::OnTrigger` (Gothic2.exe `0x0071e7d0`) and
- `oCMobInter::OnUntrigger` (Gothic2.exe `0x0071eac0`).

That means a mob (lever / switch / door / pressure-plate / fire) is itself a valid
*trigger target*: when any trigger or script names it, the engine delivers `OnTrigger`
to it. The two overrides do two distinct things, certain from the decompile:

1. **No-source path (the triggering vob argument is NULL).** This is exactly what the
   script external `Wld_SendTrigger`/`oCGame::SendTrigger` produces — it fans a trigger
   out to every vob named `triggerTarget` with *no* source vob. In that case
   `OnTrigger` checks the mob is currently in state 0 and not blocked, then sets its
   target state to **1**, sets the visual state to 1, and starts the `S_S1` animation on
   its own model (`zCModel::StartAni("S_S1")`). `OnUntrigger`'s mirror requires the mob
   to be in state 1, sets the target/visual state to **0** and starts `S_S0`. This is
   the *remote activation* of the switch/door — the lever throws itself.

2. **Forward/relay path (both branches, run unconditionally).** After the optional
   self-flip, both `OnTrigger` and `OnUntrigger` enumerate the mob's own
   `triggerTarget` list (struct field at `+0x19c`) and forward `OnTrigger` /
   `OnUntrigger` to every vob named there via its event manager. So a mob also acts as a
   trigger *relay*.

`oCMobInter::CheckStateChange` (`0x00720440`) → `OnEndStateChange` (`0x00720c80`) is the
*other* entry point (interactive use by an NPC); the trigger virtuals above are the
remote/scripted entry point and are wholly separate from it.

## OpenGothic file:line

- `game/world/worldobjects.cpp:407-419` — `WorldObjects::execTriggerEvent` dispatches a
  `TriggerEvent` **only** by iterating `triggers` (the `AbstractTrigger` list); it then
  falls through to world-sound only. There is no branch that consults
  `interactiveObj`.
- `game/world/worldobjects.cpp:516-517` — `addTrigger(AbstractTrigger*)`: the `triggers`
  list is exclusively `AbstractTrigger` instances. Mobs live in the separate
  `interactiveObj` collection (`addInteractive`, line 704).
- `game/world/objects/interactive.h:17,57` — `class Interactive : public Vob`. It
  declares only `emitTriggerEvent` (outgoing); it overrides **no** `onTrigger` /
  `processEvent` (grep confirms: the only trigger member on `Interactive` is the
  outgoing emitter).
- `game/game/gamescript.cpp:1953-1958` — `Wld_SendTrigger` builds a `TriggerEvent` with
  an **empty source name** (`""`, i.e. the original's NULL-source path) and routes it
  through `world.triggerEvent` → `execTriggerEvent`.

## Divergence

A `TriggerEvent` whose `target` names a MOBSI is silently dropped in OpenGothic:
`execTriggerEvent` only matches `AbstractTrigger` objects, and `Interactive` is not one
and registers no trigger handler. Concretely:

- A Daedalus script that does `Wld_SendTrigger("DOOR_TO_CASTLE")` /
  `Wld_SendTrigger("LEVER_X")` to remotely open a door or throw a switch does **nothing**
  in OpenGothic. In the original this lands on the NULL-source `OnTrigger` path and flips
  the mob to state 1 (`S_S1`), `Wld_SendUntrigger` flips it back (`S_S0`).
- A trigger/trigger-list whose `triggerTarget` is a MOBSI loses both the self-flip and
  the relay-to-the-mob's-own-target behaviour.

The empty source name produced by `Wld_SendTrigger` (`gamescript.cpp:1957`) maps exactly
to the original's state-flipping NULL-source branch, so this is the most directly
script-observable half of the gap.

## Proposed patch

DEFERRED. Direction (for a future change), with citation:

```
// NOTE: in original-game oCMobInter::OnTrigger @0x0071e7d0 / OnUntrigger @0x0071eac0 a
// MOBSI is itself a valid trigger target: a NULL-source trigger (Wld_SendTrigger,
// gamescript.cpp:1957 emits source "") flips it state 0->1 and plays S_S1 (untrigger:
// 1->0, S_S0), and both overrides additionally forward to the mob's own triggerTarget.
```

The fix requires (a) `WorldObjects::execTriggerEvent` to also visit `interactiveObj` and,
for each `Interactive` whose `vobName==e.target`, invoke a new
`Interactive::onTrigger(TriggerEvent)`; and (b) that handler to perform the remote
state flip (mirroring the NULL-source branch) plus re-`emitTriggerEvent` to the mob's
own `triggerTarget`.

Deferred because the state-flip half (set target state 1 / play `S_S1`, or 0 / `S_S0`,
gated on the mob's current state) has to drive the same `setState` / on-state / trigger
machine whose state-index semantics are *already* flagged as divergent and deferred in
`mobstate-trigger-onstate-state-index.md`; wiring remote activation through it before
that base machine is settled risks compounding the off-by-one rather than a clean,
build-verifiable fix. The dispatch-and-relay half alone is safe but incomplete, so the
whole finding is held as DEFERRED rather than shipping a partial behavior.
