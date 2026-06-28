# perc4: ASSESSDRAWWEAPON(24) / ASSESSREMOVEWEAPON(11) / ASSESSWARN(15) / ASSESSUSEMOB(32) — NO MISSING BROADCAST (one over-fire noted)

**Confidence:** NO FINDING for a *missing* broadcast. The one real divergence found is an
**over-broadcast** of PERC_ASSESSUSEMOB (Medium confidence), which is the opposite bug class
to the one requested, so it is reported as an observation, not applied.

## Original functions + addresses (prose)

The original broadcasts each candidate from exactly one engine site, all via
`oCNpc::CreatePassivePerception` @0x0075b270:

- **ASSESSDRAWWEAPON(24)** — `oCNpc::EV_DrawWeapon2` @0x0074d580 fires
  `CreatePassivePerception(self, 24, self, NULL)` in its "weapon already at target / instant"
  branch, gated on virtual slot **vtable+0x100 = `oCNpc::IsAPlayer` @0x007425a0** (`this==player`).
  i.e. player-only.
- **ASSESSREMOVEWEAPON(11)** — `oCNpc::EV_RemoveWeapon2` @0x0074e630 fires
  `CreatePassivePerception(self, 11, self, NULL)`, gated on virtual slot
  **vtable+0x104 = `oCNpc::IsSelfPlayer` @0x007425b0** (`this==player`). Player-only.
- **ASSESSUSEMOB(32)** — `oCNpc::AssessUseMob_S` @0x0075d300 fires
  `CreatePassivePerception(self, 32, self, NULL)`, gated on **vtable+0x104 = `IsSelfPlayer`**
  (`this==player`). Player-only. Its two callers — `oCAIHuman::CreateAssessUseMob` @0x0069b260
  (AI tick, runs for every human NPC) and `CheckMobInteraction` @0x006983ad — both funnel through
  this player-only gate, so the engine only broadcasts USEMOB when the *player* uses a mob.
- **ASSESSWARN(15)** — `oCNpc::AssessWarn_S` @0x0075c510 fires
  `CreatePassivePerception(self, 15, other, NULL)` but has **no engine caller** (`wde xrefs`
  returns none). It is dead engine code reachable only externally; not an engine broadcast.

Vtable base verified as `oCNpc::_vftable_` = 0x0083d724: slot+0x100→IsAPlayer(0x83d824),
slot+0x104→IsSelfPlayer(0x83d828), slot+0x110→IsHuman(0x83d834), slot+0xd8→DoDoAniEvents
@0x00742a20 — the last matching OpenGothic's existing cited address, confirming the mapping.

## OpenGothic status (already correct)

- **PERC_DRAWWEAPON=24** (constants.h:433) — sent from `Npc::implSetFightMode`
  game/world/objects/npc.cpp:2041-2042, gated `isPlayer()`. Matches IsAPlayer gate. FAITHFUL.
- **PERC_ASSESSREMOVEWEAPON=11** (constants.h:420) — sent from `Npc::closeWeapon`
  game/world/objects/npc.cpp:4035-4036, gated `isPlayer()`. Matches IsSelfPlayer gate. FAITHFUL.
- **PERC_ASSESSWARN=15** (constants.h:424) — not sent; original has no engine caller. FAITHFUL (dead).
- **PERC_ASSESSUSEMOB=32** (constants.h:441) — sent from `Interactive::tick`
  game/world/objects/interactive.cpp:406, :421, :426. Broadcast is PRESENT, so it is **not missing**.

Verified signature: `World::sendPassivePerc(Npc& self, Npc& other, int32_t perc)` (world.h:170).

## Divergence (observation only — over-fire, not a missing broadcast)

The three OpenGothic USEMOB sites call `npc.world().sendPassivePerc(npc,npc,PERC_ASSESSUSEMOB)`
**unconditionally on the user's identity**, whereas the original `oCNpc::AssessUseMob_S` @0x0075d300
gates the broadcast on `IsSelfPlayer` (vtable+0x104, `this==player`). NPCs do step mobsis in Gothic
(scripted), so OpenGothic emits USEMOB perceptions for NPC mob use that the original never broadcasts.
This is an over-broadcast, the inverse of the requested "missing broadcast" class, and the task
rule ("If 11/15/24/32 are all faithfully fired or not-engine-fired, NO FINDING") plus
"empty beats false positives" means no surgical *missing-broadcast* patch is warranted here.

## Proposed patch

NO FINDING (no missing engine perception broadcast among 11/15/24/32).

For the record, the faithful gate for the over-fire would be to wrap each of the three USEMOB
sends in `if(npc.isPlayer())`, e.g. at game/world/objects/interactive.cpp:421:

OLD:
```cpp
  if(state==0 && p.attachMode) {
    npc.world().sendPassivePerc(npc,npc,PERC_ASSESSUSEMOB);
    emitTriggerEvent(TriggerEvent::T_Trigger);
    }
```
NEW:
```cpp
  if(state==0 && p.attachMode) {
    // NOTE: in original-game oCNpc::AssessUseMob_S @0x0075d300 the USEMOB broadcast is gated on
    // vtable+0x104 = oCNpc::IsSelfPlayer @0x007425b0 (this==player); both callers
    // (oCAIHuman::CreateAssessUseMob @0x0069b260, CheckMobInteraction @0x006983ad) funnel through
    // it, so the engine only broadcasts USEMOB when the player uses a mob.
    if(npc.isPlayer())
      npc.world().sendPassivePerc(npc,npc,PERC_ASSESSUSEMOB);
    emitTriggerEvent(TriggerEvent::T_Trigger);
    }
```
(and identically at :406 and :426). Left UNAPPLIED: it is an over-fire correction, out of scope
for this missing-broadcast hunt, and lower confidence than the requested class of fix.
