# Issue #684 — Mercenaries, outfit, talking, bug

## Issue
v1.0.2878 (Linux). Player removes mercenary outfit, talks to a mercenary (rejected for wrong clothes), re-equips the proper outfit, talks again — the mercenary "won't say any words." Repro provided as save + video only; no log.

## OG files
- game/world/objects/npc.cpp (`startDialog`, `perceptionProcess` with `PERC_ASSESSTALK`, `setRefuseTalk`/`refuseTalkMilis`)
- game/game/gamescript.cpp (dialog selection, `B_AssessTalk` script flow)
- game/game/aistate.cpp (ZS_Talk / talk-state entry)

## Original behavior (prose)
In the original, when a talk attempt is refused the script sets a refuse-talk timer (`AI_SetWalkmode`/`Npc_SetRefuseTalk`) on the target NPC. While that timer is active the NPC will not re-trigger `PERC_ASSESSTALK`. Once the timer elapses, and once the player meets the outfit condition, the next assess-talk should fire normally and start the dialog. The original keys re-eligibility off the refuse-talk timer expiring plus a fresh perception trigger, not off a one-shot flag that could latch.

## OG current file:line
- `Npc::startDialog` npc.cpp:4192, gated by `perceptionProcess(pl,nullptr,0,PERC_ASSESSTALK)` (npc.cpp:4195).
- Refuse-talk state: `refuseTalkMilis` is serialized at npc.cpp:213/267; `isTalk` at npc.cpp:4158.

## Divergence
Likely a latched refuse-talk / perception-eligibility state: after the first refusal the assess-talk perception is not re-armed when the player re-equips the outfit, so the second (now valid) talk attempt never reaches the dialog. The exact trigger requires loading the provided save and stepping the talk perception path at runtime, which cannot be confirmed statically here.

## Recommendation: DEFER
A confident surgical fix cannot be derived from source alone. Reproduction guide:
1. Load the provided save; stand near the mercenary out of outfit.
2. Add tracing in `Npc::perceptionProcess` for `PERC_ASSESSTALK` and in `startDialog`, plus the refuse-talk timer set/clear sites, logging `refuseTalkMilis` and the outfit/guild check the script uses (`B_AssessTalk`).
3. Confirm whether after re-equip (a) the perception re-fires but the script still rejects (script-condition / `trueGuild` mismatch — overlaps #656), or (b) the perception is suppressed by a stale refuse-talk timer that is never reset on outfit change.
4. If (b): reset the refuse-talk timer (or re-arm assess-talk) when the player's equipped armor changes; if (a): the guild/outfit condition is the culprit and should be triaged with #656.
