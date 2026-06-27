# PERC_OBSERVEINTRUDER is never emitted by the player (warn/threaten trigger missing)

**Confidence:** High (for the divergence). Medium (for the exact patch timing).

## Original function + address (prose only)

The player-controlled movement routine in `oAiHuman.cpp` (Ghidra label `FUN_0069ae20`,
entry `0x0069ae20`, the player walk/stand-actions path dispatched from
`oCAIHuman::Moving` @ `0x0069b9b0`) raises the **PERC_OBSERVEINTRUDER (=12)** passive
perception once each time the player comes to a stop.

The logic: the routine sets a transient "did a locomotion this frame" flag (bit `0x20`
of the AI state byte at offset `+0x1204`) whenever it issues a forward / backward /
jump / strafe move. When the player has no forward/backward input on a given frame and
the locomotion-control reports `IsStanding()` while that latch bit is set, it clears the
latch and — provided the persistent walk-mode field (offset `+0x160`) is **not** sneak
(value `2`) — calls `oCNpc::ObserveIntruder_S` @ `0x0075c460`. That wrapper in turn
calls `CreatePassivePerception(this, 0xc /*PERC_OBSERVEINTRUDER*/, this, NULL)` (verified
by decompiling `ObserveIntruder_S`). Net effect: stopping (but not while sneaking)
broadcasts "intruder observed" to nearby NPCs, which drives the guard
warn-and-threaten reaction via their `B_ObserveIntruder` handler. The emission is
latched so it fires once per stop, not every standing frame.

(For reference, the sibling sound triggers in the same family were cross-checked:
`oCAIHuman::CreateFootStepSound` @ `0x0069b180` emits `PERC_ASSESSQUIETSOUND (=14)` while
walking non-sneak — which OpenGothic already mirrors at `npc.cpp:2426` — and the sneak
branch of `FUN_0069ae20` emits `PERC_OBSERVESUSPECT (=25)`. So the perc-id mapping here
is firmly anchored: `0xc`=OBSERVEINTRUDER, `0xe`=QUIETSOUND, `0x19`=OBSERVESUSPECT.)

## OpenGothic file:line

- `game/game/constants.h:421` — `PERC_OBSERVEINTRUDER = 12` is declared.
- A repo-wide grep (`grep -rn "OBSERVEINTRUDER\|ObserveIntruder" game/`) finds the symbol
  **only** in the enum declaration. It is never sent. The player movement code in
  `game/game/playercontrol.cpp` (`implMove` @ line 607, move-anim selection around
  lines 865–887) chooses `Npc::Anim::Idle`/`Move`/`MoveBack` but never raises any
  intruder perception on the move→stand transition.

## Divergence

In the original, when the player stops walking/running near guards (and is not sneaking),
each stop broadcasts PERC_OBSERVEINTRUDER, giving nearby NPCs an extra, immediate hook to
notice and warn/threaten a lingering intruder. OpenGothic never emits PERC_OBSERVEINTRUDER
at all, so this stop-driven warn trigger is entirely absent; guards in OpenGothic only ever
notice a stationary player through the periodic active PERC_ASSESSPLAYER poll, not through
the original's "you just stopped here" passive cue.

## Proposed patch — DEFERRED (with reason + concrete sketch)

DEFERRED for auto-apply. Reason: a faithful, build-verifiable port requires reproducing
the original's **once-per-stop edge latch** (the `0x20` "was moving" bit). OpenGothic's
`PlayerControl` is anim-driven (it recomputes an `Npc::Anim` each frame) and has no
existing moving→standing edge signal, so a correct fix must add a new latch member and
emit exactly on the falling edge. A naive emit on every `Idle`/standing frame would spam
PERC_OBSERVEINTRUDER and produce repeated guard warnings — a false-positive regression
that is strictly worse than the current silence ("empty beats false positives"). The exact
edge timing cannot be validated without running guard scenes, so this is documented rather
than applied.

Sketch of the intended surgical fix (grep-verified symbols: `PlayerControl::implMove`
exists @ `playercontrol.cpp:607`; `Npc::isStanding`, `Npc::walkMode`, `WalkBit::WM_Sneak`,
`Npc::isPlayer`, and `WorldObjects::sendPassivePerc(self,other,victim,itm,perc)` @
`worldobjects.h:135` all exist and are already used elsewhere):

```cpp
// add a transient latch member to PlayerControl (header):
//   bool wasInMove = false;

// OLD (playercontrol.cpp, end of implMove move/anim handling):
//   ... existing ani selection, no intruder perception ...

// NEW (conceptual; emit on move->stand falling edge, sneak-gated, once per stop):
//   const bool moving = (ani==Npc::Anim::Move || ani==Npc::Anim::MoveBack ||
//                        ani==Npc::Anim::MoveL || ani==Npc::Anim::MoveR);
//   if(wasInMove && !moving && pl.isStanding() &&
//      (pl.walkMode()&WalkBit::WM_Sneak)!=WalkBit::WM_Sneak) {
//     // NOTE: in original-game oAiHuman.cpp player-move FUN_0069ae20 @0x0069ae20
//     // raises PERC_OBSERVEINTRUDER via oCNpc::ObserveIntruder_S @0x0075c460 once on the
//     // move->stand transition while not sneaking (walk-mode field +0x160 != 2).
//     w->sendPassivePerc(pl,pl,&pl,nullptr,PERC_OBSERVEINTRUDER);
//   }
//   wasInMove = moving;
```

Open items a real fix must settle before applying: (1) confirm the latch is set by *all*
locomotion the original counts (forward/backward/strafe/jump — the `0x20` bit is also set
in the jump and strafe paths), not just forward/back; (2) confirm the emit must be
suppressed during mobsi/AI-queue/casting early-returns above (the original is only reached
on the free player-move path); (3) verify guard scripts' `B_ObserveIntruder` reaction
matches so the added cue does not over-trigger.
