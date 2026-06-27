# MOBSI trigger / on-state fire at the wrong state index (advance vs arrive)

**Confidence:** Low–Medium (real divergence in the base state machine; DEFERRED — fix not safely surgical yet)

## Original function + address (prose only)

`oCMobInter::OnEndStateChange` (the base oCMobInter override; there is a *second*,
subclass override of the same vtable slot for the lockable/switch family that I could
not isolate via the warm decompiler) is invoked from `oCMobInter::CheckStateChange`
once a state-transition animation has finished, receiving `(npc, oldState, newState)`
where `oldState` is the visual state field (struct offset 0x1f4) and `newState` is the
target state field (offset 0x1fc). Its base behaviour is, with certainty from the
decompile:

- if `newState == 1` it calls `OnTrigger` (vtable+0x10 → fires the VOB's `triggerTarget`);
- else if `newState == 0` it calls `OnUntrigger` (vtable+0x14);
- if `oldState < newState` (an *advance*) it calls `SendCallOnStateFunc(npc, newState)`
  — i.e. the per-state script callback is invoked with the **destination** state index;
- the only *descending* call is the single `1 -> 0` step, which calls
  `SendCallOnStateFunc(npc, 0)`.

So in the original: the script `on_state` callback for state *k* fires when the NPC
**arrives** at state *k* (transition *k-1 -> k*), and the `triggerTarget` fires when the
mob **reaches state 1** (untriggers on reaching state 0) — not at state 0 nor at
`stateNum`.

## OpenGothic file:line

`game/world/objects/interactive.cpp`
- `Interactive::implTick` trigger emission: lines 403–411
  (`emitTriggerEvent(T_Trigger)` gated on `state==0 && p.attachMode`;
   `emitTriggerEvent(T_Untrigger)` gated on `state==stateNum && p.attachMode && reverseState`).
- `Interactive::invokeStateFunc` call sites: lines 417–419 (advance path) and 372–379
  (terminal path). `invokeStateFunc` (line 511) builds `onStateFunc + "_S" + state`
  using the *current* `state` member, which at line 417 is the **source** state of the
  transition committed in this tick (the `setState(state+1)` advance happens afterwards
  at lines 421–424).

## Divergence

Two related off-by-one-in-state-index mismatches in the generic state machine:

1. **on_state index.** Original fires `on_state_S{k}` on *arriving* at state *k*; OG fires
   it while *leaving* state *k* (it uses the source index and plays `T_S{k}_2_S{k+1}` in
   the same tick). For a multi-state mob (e.g. bench → sit, or a 3+ state forge wheel)
   every intermediate `on_state` script therefore runs one transition later than vanilla
   (the terminal-state callback at lines 372–374 does fire on arrival, so only the
   intermediates drift).

2. **triggerTarget timing.** Original fires the trigger when the mob reaches state **1**
   and untriggers at state **0**. OG fires `T_Trigger` at state **0** and `T_Untrigger`
   at state **`stateNum`** (while reversing). For the common single-transition switch/
   lever (states 0↔1) these two endpoints are effectively swapped relative to the base
   semantics, so a lever wired to a `triggerTarget` would signal at the start of the pull
   rather than at completion.

## Proposed patch

DEFERRED. Reasons:

- The mobs that actually carry a `triggerTarget` (levers, switches, doors:
  `oCMobSwitch`/`oCMobLockable`) override `OnEndStateChange` with a *second* vtable
  function that the warm decompiler would not surface by class-qualified name, so the
  base-class trigger states (`new==1`/`new==0`) documented above may not be the ones the
  shipping switch/door path uses. Confirming the real lever/door trigger states requires
  decompiling that subclass override first; patching against the base behaviour risks
  regressing currently-working door/switch triggering.
- The OG state base is `-1 → 0 → … → stateNum` (attach sets `state=-1`, `interactive.cpp`
  lines 878–888) and the original's initial visual-state field (0x1f4) on
  `BeginInteraction` was not pinned down here; without that alignment a state-index shift
  cannot be applied with confidence.
- The on_state region (lines 413–419) already carries a prior parity fix, so any change
  must be reconciled with that intent rather than applied blind.

A safe follow-up would: (1) decompile the subclass `OnEndStateChange`/`OnTrigger` for
`oCMobSwitch`/`oCMobLockable` and the original `BeginInteraction` to fix the state base,
then (2) re-key `emitTriggerEvent`/`invokeStateFunc` to the *destination* state index on
each committed advance.

// NOTE: in original-game oCMobInter::OnEndStateChange (base, called from
// oCMobInter::CheckStateChange) fires SendCallOnStateFunc with the destination state on
// each advance and OnTrigger/OnUntrigger on reaching state 1/0 respectively.

### Also checked, NOT a bug

`GameScript::schemeToBodystate` / `searchScheme` (`game/game/gamescript.cpp` 1452–1493)
looked divergent versus `oCMobInter::SetMobBodyState` (comma vs the original's tokenizer,
exact-match vs the original's substring `zSTRING::Search`, and a reversed
NOTINTERRUPTABLE/SIT/LIE/CLIMB precedence). It is **not** a behavioural divergence: the
shipped G2 `MOB_*` constants are comma-separated word lists
(`BENCH,CHAIR,GROUND,THRONE` … `DOOR,LEVER,…,SMOKE,INNOS`), and OG's
`ProtoMesh::setupScheme` (`protomesh.cpp` 333) pre-strips the visual to the first `_`
token (`DOOR_WOODEN→DOOR`, `CHESTBIG_NW_…→CHESTBIG`), so the OG scheme exactly equals the
list word in every vanilla case — exact-match is equivalent to the original substring
search, and since the four lists are word-disjoint the precedence order is irrelevant.
