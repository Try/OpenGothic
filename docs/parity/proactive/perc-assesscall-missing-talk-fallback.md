# PERC_ASSESSCALL never fired: missing ASSESSTALK->ASSESSCALL fallback on player interact

**Confidence:** High

## Original fn + address

When the player targets and activates an NPC, the original runs the talk/call
branch of `oCAIHuman::StandActions` (Gothic2.exe @0x00698ea0). For an alive,
non-unconscious focused NPC it does:

1. If the NPC **has** `PERC_ASSESSTALK` (0x13) **and**
   `oCNpc::IsInPerceptionRange(0x13, player, npc)` (@0x0075e490, real distance <
   percRange[0x13]) is true: if `oCNpc::CanBeTalkedTo` (@0x006bd060), invoke
   `oCNpc::AssessTalk_S` (@0x0075c890) and stop.
2. **Otherwise** (NPC lacks `PERC_ASSESSTALK` **or** is out of talk range): if
   the NPC **has** `PERC_ASSESSCALL` (0x12) **and**
   `IsInPerceptionRange(0x12, player, npc)` is true, invoke
   `oCNpc::AssessCall_S` (@0x0075c6f0) and stop.

`AssessCall_S` is the "hailing" reaction (e.g. an NPC the player targets from a
distance calls out / turns toward the player) and is gated by its own
`percRange[0x12]` entry, distinct from the talk range. The four call sites of
`IsInPerceptionRange` in the binary are `StandActions` (the 0x13 and 0x12 calls
here), `EV_Ask` and `FUN_00699f60` — only the talk side was previously mirrored.

## OG file:line

`game/world/objects/npc.cpp:4527` — `Npc::startDialog`, reached from
`PlayerControl::interact(Npc&)` at `game/game/playercontrol.cpp:358`. The sibling
fix corrected the `PERC_ASSESSTALK` distance gate here, but the `else` branch —
the `PERC_ASSESSCALL` fallback — is entirely absent. `PERC_ASSESSCALL` (= 18,
`game/game/constants.h:427`) is defined in the enum but is **never** passed to
`perceptionProcess` / `sendPassivePerc` anywhere in `game/` (grep: zero trigger
sites). So a focused NPC that lacks B_AssessTalk or is beyond talk range but has
B_AssessCall within call range produces no reaction in OpenGothic, whereas the
original makes it hail the player.

## Divergence

`startDialog` only attempts `PERC_ASSESSTALK`; when that does not fire (no talk
perc or out of talk range) the original tries `PERC_ASSESSCALL` with its own
range gate. OpenGothic omits this second branch, so `AssessCall_S`-driven
behavior never runs.

## Proposed patch

`game/world/objects/npc.cpp`, in `Npc::startDialog`:

OLD:
```cpp
  if(perceptionProcess(pl,nullptr,pl.qDistTo(*this),PERC_ASSESSTALK))
    setOther(&pl);
  }
```

NEW:
```cpp
  // NOTE: in original-game oCAIHuman::StandActions @0x00698ea0 a player-initiated
  // interact tries PERC_ASSESSTALK (AssessTalk_S @0x0075c890) first; only if the NPC
  // lacks PERC_ASSESSTALK or is out of its range does it fall back to PERC_ASSESSCALL
  // (AssessCall_S @0x0075c6f0), gated by IsInPerceptionRange(0x12,player,npc) @0x0075e490
  // against the separate percRange[0x12] (Perc_SetRange) entry. OpenGothic only ran the
  // talk path, so PERC_ASSESSCALL (B_AssessCall hailing reaction) never fired and the
  // perc id was unused. The talk perceptionProcess returns false in exactly the two
  // cases the original branches on (no perc / quadDist>range), so mirror with an else.
  if(perceptionProcess(pl,nullptr,pl.qDistTo(*this),PERC_ASSESSTALK))
    setOther(&pl);
  else
    perceptionProcess(pl,nullptr,pl.qDistTo(*this),PERC_ASSESSCALL);
  }
```

Note: the `CanBeTalkedTo` extra-condition on the talk branch is a separate
pre-existing gap (OpenGothic's `perceptionProcess(PERC_ASSESSTALK)` invokes the
talk state without it) and is intentionally left untouched here; this patch only
restores the missing ASSESSCALL fallback. `AssessCall_S`, like `AssessTalk_S`,
sets OTHER=player via `StartAIState`, which `perceptionProcess`'s `invokeState`
already does (`&pl` as other), so no extra `setOther` is required for the call
path.
