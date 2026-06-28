# Talk-start bypasses the PERC_ASSESSTALK perception-range gate

**Confidence:** High (engine logic is explicit and verifiable; exact behavioral
window depends on the mod's `Perc_SetRange(PERC_ASSESSTALK, …)` value, but the
missing gate itself is unambiguous).

## Original function + address

When the player presses the action key on a focused, living NPC, the original
routes through `oCAIHuman::StandActions` (Gothic2.exe `@0x00698ea0`). For a
focused NPC (`oCNpc::GetFocusNpc`) that is not dead/unconscious, it evaluates, in
order:

1. `oCNpc::HasPerception(npc, 0x13)` — `0x13` = 19 = `PERC_ASSESSTALK`, **and**
2. `oCNpc::IsInPerceptionRange(0x13, player, npc)` (`@0x0075e490`), which computes
   `zCVob::GetDistanceToVob(player, npc)` (`@0x0061b910`, real 3D origin-to-origin
   distance) and returns true only when that distance is **strictly less than**
   the global `percRange[0x13]` entry, **and**
3. `oCNpc::CanBeTalkedTo(npc)` (`@0x006bd060`).

Only if all three hold does it call `oCNpc::AssessTalk_S(npc, player)`
(`@0x0075c890`, the engine side of the `B_AssessTalk` perception). If the
ASSESSTALK perception/range fails, it falls through to the
`PERC_ASSESSCALL` (`0x12` = 18) branch / `AssessCall_S` instead — it does **not**
start a conversation. So in the original, *being able to focus an NPC is not
sufficient to talk to it: the player must also be inside the NPC's
`PERC_ASSESSTALK` perception range.*

This is a distinct, earlier engine-level gate from the already-deferred
`AI_ProcessInfos` 2000u drop (that one fires downstream, inside the script's
`B_AssessTalk` → `AI_ProcessInfos`).

## OG file:line

`game/world/objects/npc.cpp:4520-4525`

```cpp
void Npc::startDialog(Npc& pl) {
  if(pl.isDown() || pl.isInAir() || isPlayer())
    return;
  if(perceptionProcess(pl,nullptr,0,PERC_ASSESSTALK))
    setOther(&pl);
  }
```

Reached from `PlayerControl::interact(Npc&)` (`game/game/playercontrol.cpp:358`)
when the player actions a focused NPC. NPCs are focusable out to
`policy.npc_longrange` while unarmed (`game/world/world.cpp:426-429`), which is
typically far larger than the ASSESSTALK perception range, so the gate is not
redundant with the focus range.

## Divergence

`startDialog` passes the distance argument as a hard-coded `0`. Inside
`Npc::perceptionProcess(pl,victim,quadDist,perc)`
(`game/world/objects/npc.cpp:4577-4600`) the range test is:

```cpp
float r = float(world().script().percRanges().at(perc, hnpc->senses_range));
r = r*r;
if(quadDist>r)
  return false;
```

With `quadDist == 0`, `0 > r*r` is always false, so the `PERC_ASSESSTALK`
perception-range gate is **never applied to player-initiated talk**. OpenGothic
therefore lets the player start a conversation from any distance at which the NPC
can be focused, whereas the original requires the real player↔NPC distance to be
within `percRange[PERC_ASSESSTALK]`. (`PerDist::at` falls back to `senses_range`
only when the mod never called `perc_setrange` for ASSESSTALK — the same
`perc_setrange` binding the original feeds into `percRange`, so when the script
sets it the two paths agree.)

`PERC_ASSESSTALK == 19 == 0x13` is confirmed in `game/game/constants.h:428`,
matching the `0x13` literal in `StandActions`.

## Proposed patch

Pass the real squared distance so the existing range gate runs, exactly as the
original `IsInPerceptionRange(PERC_ASSESSTALK, …)` check does. `Npc::qDistTo` is
the same squared-distance helper `perceptionProcess` consumes for every other
perception, so this is the OG-idiomatic value.

OLD (`game/world/objects/npc.cpp:4520`):
```cpp
void Npc::startDialog(Npc& pl) {
  if(pl.isDown() || pl.isInAir() || isPlayer())
    return;
  if(perceptionProcess(pl,nullptr,0,PERC_ASSESSTALK))
    setOther(&pl);
  }
```

NEW:
```cpp
void Npc::startDialog(Npc& pl) {
  if(pl.isDown() || pl.isInAir() || isPlayer())
    return;
  // NOTE: in original-game oCAIHuman::StandActions @0x00698ea0 a player-initiated
  // talk is gated by oCNpc::IsInPerceptionRange(PERC_ASSESSTALK,player,npc)
  // @0x0075e490 (real distance < percRange[0x13]) before AssessTalk_S @0x0075c890
  // is invoked; an out-of-range NPC is not talked to. Pass the true squared
  // distance so perceptionProcess applies the same PERC_ASSESSTALK range gate
  // instead of the bypassing 0.
  if(perceptionProcess(pl,nullptr,pl.qDistTo(*this),PERC_ASSESSTALK))
    setOther(&pl);
  }
```

Note: the original additionally requires `oCNpc::CanBeTalkedTo` (NPC in `ZS_TALK`
and not already mid-conversation). That secondary eligibility check is **not**
reimplemented here (OpenGothic guards the in-conversation case via other state)
and is left as a separate, lower-confidence item — this patch is scoped strictly
to the missing distance gate.
