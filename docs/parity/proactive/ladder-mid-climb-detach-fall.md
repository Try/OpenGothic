# Ladder: letting go mid-climb should drop the player (S_FALLDN), not snap to stand

**Confidence:** High (on the original-game behavior; the OG-side fix is scoped as DEFERRED — see below)

## Original function + address

`oCMobLadder::Interact` (Gothic2.exe @0x00727a60), with helper
`oCMobLadder::CanChangeState` (@0x00727990) and the parsed step count set in
`oCMobLadder::StartInteraction` (@0x007277d0).

In prose:

- `StartInteraction` parses the trailing number of the ladder's visual name and
  stores `count - 1` as the top-rung index (mob field +0x1F8). The current rung is
  field +0x1F4 ("state"). Mounting at the bottom starts at state 0, at the top at
  the max rung.
- `oCMobLadder::CanChangeState(npc, fromState, toState)` overrides the base mob
  rule: a transition is allowed only if the base `oCMobInter::CanChangeState`
  passes AND (the transition is *not* a detach, i.e. `toState != -1`) OR the
  current rung is the very bottom (state == 0) OR the very top (state == max
  rung +0x1F8). In other words **the engine refuses a clean detach while the
  player is on a middle rung.**
- `oCMobLadder::Interact` has a dedicated early branch: when a detach/quit has been
  requested (mob "use-key" flag +0x234 set) on a fresh action-key event and the
  rung is within [0, maxRung], it calls `CanChangeState(npc, rung, -1)`:
  - if that returns non-zero (bottom or top), it does the normal clean dismount
    (`SendStateChange(state, -1)` then `EndInteraction(npc, 1)` — the
    `T_LADDER_..._S<n>_2_STAND` transition);
  - **if it returns zero (mid-ladder), the original makes the player fall off:** it
    calls `oCNpc::Interrupt`, re-enables the NPC's physics/rigid-body and gravity,
    zeroes the rigid-body velocity, plays the global player anim **`S_FALLDN`**
    (string @0x0089EDAC) directly via `zCModel::StartAni`, and ends the
    interaction. The player drops to the ground from wherever they were on the
    ladder.

So in vanilla, pressing the action/use key partway up a ladder does NOT cleanly
exit the climb in place — the character lets go and falls (`S_FALLDN`).

## OpenGothic file:line

- `game/world/objects/interactive.cpp:313-318` — `Interactive::onKeyInput`,
  `ActionGeneric` branch.
- `game/game/playercontrol.cpp:987-998` — `PlayerControl::implMoveMobsi`, which
  forwards `KeyCodec::ActionGeneric` to `onKeyInput` unconditionally while on a
  ladder.
- Anim plumbing that the original uses is already present:
  `AnimationSolver::Anim::Fall` (`game/graphics/mesh/animationsolver.h:33`) maps to
  `S_FALLDN` (`game/graphics/mesh/animationsolver.cpp:334`).

## Divergence

OG `Interactive::onKeyInput` handles `ActionGeneric` unconditionally:

```cpp
if(act==KeyCodec::ActionGeneric) {
  npc.setInteraction(nullptr);
  npc.stopAnim("");
  p->user = nullptr;
  return;
  }
```

This detaches from the ladder at **any** rung with a clean in-place exit (no fall),
ignoring the original's rung gate. In the original, the same key press on a middle
rung is rejected by `oCMobLadder::CanChangeState(state, -1)` and routed to the
fall-off path (re-enable physics, zero velocity, play `S_FALLDN`). The OG behavior
lets the player teleport-stand off the side of a ladder mid-climb; vanilla makes
them drop.

Grep-verified OG symbols that exist and are relevant: `Interactive::isLadder()`,
`Interactive::state`, `Interactive::stateNum`, `Interactive::stepsCount`,
`Npc::setInteraction`, `Npc::quitInteraction`, `Npc::setAnim`,
`AnimationSolver::Anim::Fall` (→ `S_FALLDN`).

## Proposed patch

DEFERRED — root cause is identified and high-confidence, but a correct,
build-verifiable surgical fix is not yet isolated:

1. The "mid-ladder" predicate must mirror the original exactly. Vanilla allows a
   clean detach only at `state==0` (bottom) or `state==maxRung`. OG's ladder state
   bookkeeping uses `state` in the range `-1..stateNum` with the top-detach already
   special-cased at `state>=stepsCount-1` (interactive.cpp:382) and mount state set
   to `stepsCount` (interactive.cpp:863-866). Pinning the exact OG `state` value
   that corresponds to vanilla's "bottom rung 0" vs "middle" vs "top maxRung"
   requires confirming the off-by-one between OG's `stepsCount`/`stateNum` (= parsed
   N) and vanilla's `+0x1F8` (= N-1) so the fall is triggered for exactly the right
   rungs and never at the legitimate top/bottom dismount.

2. The fall itself is a multi-step state change (detach interaction, restore NPC
   gravity/physics, zero velocity, drive `Anim::Fall`/`S_FALLDN`, hand control back
   to the fall/`mvAlgo` path). The interactive layer detaches via
   `npc.setInteraction(nullptr)`; wiring a forced `Anim::Fall` + physics handoff
   through that detach without regressing the normal in-place ladder dismount needs
   a focused implementation and in-engine verification (climb up a ladder, press
   action on a middle rung, confirm the player drops with `S_FALLDN` and takes/does
   not take fall damage consistent with vanilla). Feel-tuned fall damage / landing
   is explicitly out of scope per the "feel-tuning → DEFERRED" rule.

Recommended next step: add a rung gate in `Interactive::onKeyInput` (or in
`PlayerControl::implMoveMobsi`'s ladder branch) so that `ActionGeneric` on a
non-terminal ladder rung triggers a fall instead of a clean detach, citing
`// NOTE: in original-game oCMobLadder::Interact @0x00727a60 / CanChangeState
@0x00727990 a mid-ladder detach is rejected and the player falls (S_FALLDN);
only state 0 (bottom) and the top rung allow a clean dismount.` Land this only
after the off-by-one in step indexing is confirmed and the fall handoff is
verified in-engine.
