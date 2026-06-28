# perc2 — Engine-injected PERC_ASSESSFIGHTSOUND broadcast on every melee hit/parade

**Confidence:** High (divergence airtight; fix is strictly additive-removal, cannot drop OG below the vanilla baseline)

## Original fn + address

`oCNpc::CreatePassivePerception` @0x0075b270 is the *only* engine routine that delivers a
"passive" perception (perc id >= 6) to nearby witnesses — it builds a vob list of radius
`percRange[perc]`, then for each in-range `oCNpc` runs `StartAIState(handler)` provided the
candidate is not the sender, not dead (`oCNpc+0x1b8 >= 1`), and not the player.

I enumerated **every** engine caller of @0x0075b270 (via `wde xrefs 0x0075b270`) and read off the
perc id each one passes:

- DoDie → 6 (MURDER), DropUnconscious → 7 (DEFEAT), AssessDamage_S / AssessOthersDamage_S → 9
  (OTHERSDAMAGE), EV_RemoveWeapon2 / AssessRemoveWeapon_S → 11 (REMOVEWEAPON),
  ObserveIntruder_S → 12 (OBSERVEINTRUDER), AssessWarn_S → 15 (WARN),
  AssessTheft_S / DoTakeVob / CheckForOwner → 17 (THEFT), AddItemEffects / AssessFakeGuild_S → 21
  (FAKEGUILD), EV_DrawWeapon2 → 24 (DRAWWEAPON), the player-move controller FUN_0069ae20 → 25
  (OBSERVESUSPECT), Invest / AssessCaster_S → 29 (CASTER), EndTimedEffect → 30 (SURPRISE),
  AssessEnterRoom_S → 31 (ENTERROOM), AssessUseMob_S → 32 (USEMOB). The relay handlers
  AssessMurder_S/AssessDefeat_S re-emit 6/7. The only variable-perc caller is `Npc_SendPassivePerc`
  (FUN_006f3280, the Daedalus external).

**Perc id 13 (PERC_ASSESSFIGHTSOUND) is broadcast by NO engine function.** It is also not delivered
via the damage/hit/sound path: `OnDamage_Hit`/`OnDamage_Sound`/`StartHitSound`/`FightMelee`/
`EV_Parade` never `StartAIState` a perc-13 handler (their `0xd`/`0x13` constants are anim/state ids).
The `ASSESSFIGHTSOUND` name string @0x008b84c4 is reachable only through the perc-name table used by
`Npc_PercEnable`/`Npc_SendPassivePerc`. So in the original, ASSESSFIGHTSOUND is delivered *only* when
a script calls `Npc_SendPassivePerc(self, PERC_ASSESSFIGHTSOUND, ...)`; the engine itself never emits
it.

## OG file:line

`/Users/admin/Downloads/opengothic/game/world/objects/npc.cpp:2141`, inside
`Npc::takeDamage(Npc& other, const Bullet* b)` (the melee hit/parade registration path).

## Divergence

OpenGothic injects an engine-side broadcast:
```
owner.sendPassivePerc(*this,other,*this,PERC_ASSESSFIGHTSOUND);
```
on **every** registered melee strike *and* every parade (it runs before the block/jump-back
decision), for any victim. The original engine emits ASSESSFIGHTSOUND from no C++ site at all.

Because the vanilla engine never broadcasts perc 13, OG's line 2141 is purely *additive* over
vanilla: it makes nearby NPCs run their B_AssessFightSound handler on hits/parries that vanilla
engine-side never reports. Removing it can only move OG toward the vanilla baseline, never below it —
any genuinely script-driven FIGHTSOUND still flows independently through
`GameScript::npc_sendpassiveperc` → `world().sendPassivePerc` (gamescript.cpp:2740), which OG
implements identically. (Vanilla guards still *join* visible fights via the active ASSESSENEMY/
ASSESSFIGHTER loop, which is untouched by this; FIGHTSOUND is the hearing channel and stays
script-gated.)

## Proposed patch (npc.cpp ~2138)

OLD:
```
  lastHit = &other;
  if(!isPlayer())
    setOther(&other);
  owner.sendPassivePerc(*this,other,*this,PERC_ASSESSFIGHTSOUND);

  if(!(isBlock || isJumpb) || b!=nullptr || flyAtk) {
```

NEW:
```
  lastHit = &other;
  if(!isPlayer())
    setOther(&other);
  // NOTE: in original-game Gothic2.exe no engine routine broadcasts PERC_ASSESSFIGHTSOUND (13).
  // oCNpc::CreatePassivePerception @0x0075b270 is the sole passive-perc delivery path, and across
  // all of its engine callers (DoDie 6, DropUnconscious 7, AssessDamage_S 9, EV_RemoveWeapon2 11,
  // ObserveIntruder_S 12, AssessWarn_S 15, AssessTheft_S 17, AddItemEffects 21, EV_DrawWeapon2 24,
  // FUN_0069ae20 25, AssessCaster_S 29, EndTimedEffect 30, AssessEnterRoom_S 31, AssessUseMob_S 32)
  // perc 13 is never passed; the melee/hit path (OnDamage_Hit/_Sound, StartHitSound) never
  // StartAIStates a perc-13 handler either. ASSESSFIGHTSOUND is emitted only by the script external
  // Npc_SendPassivePerc. OpenGothic injected an engine broadcast here on every melee hit AND parade,
  // over-firing the perception vs vanilla; drop it (script-sent FIGHTSOUND still routes through
  // GameScript::npc_sendpassiveperc -> world().sendPassivePerc).

  if(!(isBlock || isJumpb) || b!=nullptr || flyAtk) {
```
