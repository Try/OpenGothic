# Switch / lever / door state machine — survey (NO FINDING; one DEFERRED sub-note)

**Confidence:** NO FINDING (no fresh high-confidence surgical divergence). One sub-threshold
DEFERRED observation recorded for completeness.

## Original functions + addresses (prose only)

The oCMobSwitch / oCMobLever / oCMobDoor families all reuse the **base** `oCMobInter`
state machine — neither `oCMobSwitch` nor `oCMobDoor` overrides any state-transition
vtable slot (their own methods are only ctor/dtor/Archive/Unarchive/CreateNewInstance/
GetScemeName/SearchFreePosition; `oCMobDoor::Open`/`Close` @0x0071a430/0x0071a440 are
3-byte stubs). The engine logic examined:

- `oCMobInter::oCMobInter` (0x0071d010): confirms field layout — current/visual state
  `+0x1f4` init **0**, target state `+0x1fc` init 0, **stateNum `+0x1f8` init 1**, rewind
  flag `+0x200` init 0, interaction-counter `+0x20e`, temp/routine state `+0x20f`,
  temp-dirty `+0x210`, reserved-by ptr `+0x22c`.
- `oCMobInter::Interact` (0x0071f210): per-tick player/NPC driver (direction via
  GetDirection `+0xbc`/SetDirection `+0xc0`, gating via `CanChangeState` `+0x104`).
- `oCMobInter::CanChangeState` (0x0071fc40): a transition is allowed iff a transition
  animation exists (mob model or NPC); also performs the use-with-item removal, but only
  on the `fromState==0` step.
- `oCMobInter::CheckStateChange` (0x00720440): commits a settled transition, then calls
  `OnEndStateChange(npc, from, to)` (`+0x110`).
- `oCMobInter::OnEndStateChange` (0x00720c80): on reaching `to==1` calls `OnTrigger`
  (vtable `+0x10`, verified by the vtable-adjacent xrefs 0x0083c924/…c928 to
  `OnTrigger`@0x0071e7d0 / `OnUntrigger`@0x0071eac0); on `to==0` calls `OnUntrigger`
  (`+0x14`); fires `CallOnStateFunc(npc, to)` (`+0x118`) on an ascending step and on the
  single `1->0` step.
- `oCMobInter::OnTick` (0x0071e750): the rewind auto-return — when the rewind flag
  `+0x200` is set and no NPC is interacting (`+0x20e < 1`), commits via
  `CheckStateChange(NULL)` and, **while current state `+0x1f4` > 0**, steps down one state
  (`SendStateChange(cur, cur-1)`), i.e. it returns to **state 0 and stops**.
- `oCMobInter::SetTempState`/`SetStateToTempState` (0x0071d540/0x0071d590): the
  `Wld_SetMobRoutine` path, applied via the `OnTrigger`/`OnUntrigger` self-flip
  (`zCModel::StartAni("S_S1"/"S_S0")`), deferred until the mob is idle.

## OpenGothic mapping (verified faithful)

`game/world/objects/interactive.cpp` / `interactive.h`:

- State model: OG `state` defaults to **-1** (`interactive.h:180`), `stateNum =
  inter.state` (`:45`). This is a deliberate **one-step offset** from the original
  (current state default 0): OG state `-1` ≡ original state `0` — both render `S_S0`
  (`setAnim`, `:1090-1099`). The rewind auto-return (`tick`, `:289-302`) therefore
  returns to OG `-1`, which is the original's resting `0`. Consistent, not a divergence.
- Transition gating, direction toggle (`attach` `:921-926`; `onKeyInput` `:337-341`),
  use-with-item, distance, lock/key handling all match the decompiled originals (and
  several are already individually NOTE-cited in the file).
- Routine snap (`setMobState` `:530-544`) plays `S_S<st>`, matching the original
  `OnTrigger` self-flip `StartAni("S_S1")` (the original does **not** play the
  `T_..._2_...` transition for the routine path either).

## Why NO FINDING

Every genuine divergence I could substantiate in this state machine is **already
documented/deferred**, and re-treading them is explicitly excluded:

- Trigger fires at the wrong state index (OG emits `T_Trigger` at `state==0` / `T_Untrigger`
  at `stateNum`; original fires on reaching state 1 / 0) — `mobstate-trigger-onstate-state-index.md` (DEFERRED).
- A MOBSI ignoring an incoming trigger / not relaying to its `triggerTarget` —
  `mobtrig-mobsi-ignores-incoming-trigger.md` (DEFERRED); the `Wld_SetMobRoutine`
  triggerTarget-forward (`SetStateToTempState`→`OnTrigger`) is the same trigger-forward
  machinery and is not safely surgical in isolation.
- on_state callback index / per-NPC gating — `mobstate-npc-onstate.md`,
  `onstate-multistate-walk-count.md` (handled/deferred).

The remaining mechanics (rewind rest position, routine snap animation, direction toggle,
CanChangeState animation gate, item gate, distances) are faithful or are intentional
OpenGothic representation choices (the `-1` baseline). Empty beats false positives.

## DEFERRED sub-observation (not the deliverable; low confidence)

`Interactive::tick` rewind branch (`interactive.cpp:289-302`) decrements `state` **once
per attach-position** inside a single `for(auto& i:attPos)` pass:

```
  if(p==nullptr) {
    const int destSt = -1;
    for(auto& i:attPos) {
      if(destSt!=state && (i.started==Quit || rewind)) {
        if(!setAnim(nullptr,Anim::Out))
          return;
        setState(state-1);
        i.started = NotStarted;
        }
      }
    return;
    }
```

For a **multi-seat** mob (multiple `ZS_POS*`) with `rewind`, this collapses several states
in one tick (the top-of-`tick` `world.tickCount()<waitAnim` guard `:279` only blocks
*entry*, not the inner loop), whereas the original `OnTick` steps exactly one state per
settled animation. Single-seat rewind levers/switches (the common case) are unaffected, so
impact is marginal and the safe fix (decrement once per tick, not once per seat) is not
clearly surgical — DEFERRED, not proposed.
