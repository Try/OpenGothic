# PERC_ASSESSOTHERSDAMAGE witness broadcast wrongly gated on net damage value

**Confidence:** Medium-High

## Original function + address (prose only)

In the original game, the per-hit damage pipeline runs through `oCNpc::OnDamage`
(orchestrator @ `0x006660e0`), which dispatches sub-handlers in order:
`OnDamage_Hit` (`0x00666610`), `OnDamage_Condition` (`0x0066cf30`), then — only when the
descriptor "hit landed" flag is set (bit 0 of `oSDamageDescriptor` field `+0x90`) —
`OnDamage_Anim`, `OnDamage_Effects_Start`, and `OnDamage_Script` (`0x0066e220`).

`OnDamage_Script` calls `oCNpc::AssessDamage_S` (`0x0075c280`) provided the descriptor's
"suppress assessment" flag (bit 2 / `+0x90 & 4`, set by `OnDamage_Condition` only when the
victim was already dead) is clear. `AssessDamage_S` does two things, in this order:

1. Scans the victim's perception table for `PERC_ASSESSDAMAGE` (id 8); if active it sets the
   parser `OTHER` instance to the attacker and starts that AI state on **self** (the victim).
2. Unconditionally — on a label reached by both branches at its tail — calls
   `oCNpc::CreatePassivePerception(this, 9 /*PERC_ASSESSOTHERSDAMAGE*/, attacker, self)`
   (`0x0075b270`), which broadcasts the witness perception to nearby NPCs within
   `percRange[9]`.

Critically, the damage **value** passed into `AssessDamage_S` is never consulted: both the
self `PERC_ASSESSDAMAGE` and the witness `PERC_ASSESSOTHERSDAMAGE` fire together purely on
"the hit landed and the victim was not already dead". A blow that connects but is fully
absorbed by armour (net damage 0) still alerts bystanders.

## OpenGothic file:line

`game/world/objects/npc.cpp:2139-2193` (`Npc::takeDamage(Npc&, const Bullet*, CollideMask, int32_t, bool)`)

- Self `PERC_ASSESSDAMAGE` is raised at line 2140 (`perceptionProcess(other,this,0,PERC_ASSESSDAMAGE)`).
- Witness `PERC_ASSESSOTHERSDAMAGE` is raised at line 2180, but **nested inside
  `if(hitResult.value>0)`** (line 2175).

## Divergence

OpenGothic raises the self `PERC_ASSESSDAMAGE` whenever the (non-blocked, non-good-spell)
hit path runs, but raises the witness `PERC_ASSESSOTHERSDAMAGE` only when
`hitResult.value>0`. A landed hit that deals zero net damage (target protection fully
absorbs it — `swordDamage` returns `Val(0,true,false)`, so `hasHit==true`, `value==0`) makes
the victim react via `PERC_ASSESSDAMAGE` but leaves nearby NPCs unaware: no
`PERC_ASSESSOTHERSDAMAGE` is broadcast. In the original both perceptions are issued together
from `AssessDamage_S`, independent of the net damage value, so bystanders/allies still react
to a fully-absorbed strike. (`PERC_ASSESSDEFEAT`/`PERC_ASSESSMURDER`/AARGH legitimately stay
under `value>0` — they depend on the HP change applied just above.)

## Proposed patch

`game/world/objects/npc.cpp`, in `Npc::takeDamage(...CollideMask bMask...)`:

OLD:
```cpp
  if(hitResult.value>0) {
    currentOther = &other;
    changeAttribute(ATR_HITPOINTS,-hitResult.value,dontKill);

    if(bMask&(COLL_APPLYVICTIMSTATE|COLL_DOEVERYTHING)) {
      owner.sendPassivePerc(*this,other,*this,PERC_ASSESSOTHERSDAMAGE);
      if(isUnconscious()){
        owner.sendPassivePerc(*this,other,*this,PERC_ASSESSDEFEAT);
        }
      else if(isDead()) {
        owner.sendPassivePerc(*this,other,*this,PERC_ASSESSMURDER);
        }
      else {
        if(owner.script().rand(2)==0) {
          emitSoundSVM("SVM_%d_AARGH");
          }
        }
      }
    }
```

NEW:
```cpp
  // NOTE: in original-game oCNpc::AssessDamage_S (Gothic2.exe 0x0075c280) the witness
  // broadcast CreatePassivePerception(PERC_ASSESSOTHERSDAMAGE) is issued together with the
  // self PERC_ASSESSDAMAGE whenever the hit lands (oSDamageDescriptor +0x90 bit0), with no
  // dependency on the net damage value. A landed hit fully absorbed by armour (value==0)
  // still alerts bystanders; gating it on hitResult.value>0 suppressed that witness reaction.
  if(hitResult.hasHit && (bMask&(COLL_APPLYVICTIMSTATE|COLL_DOEVERYTHING)))
    owner.sendPassivePerc(*this,other,*this,PERC_ASSESSOTHERSDAMAGE);

  if(hitResult.value>0) {
    currentOther = &other;
    changeAttribute(ATR_HITPOINTS,-hitResult.value,dontKill);

    if(bMask&(COLL_APPLYVICTIMSTATE|COLL_DOEVERYTHING)) {
      if(isUnconscious()){
        owner.sendPassivePerc(*this,other,*this,PERC_ASSESSDEFEAT);
        }
      else if(isDead()) {
        owner.sendPassivePerc(*this,other,*this,PERC_ASSESSMURDER);
        }
      else {
        if(owner.script().rand(2)==0) {
          emitSoundSVM("SVM_%d_AARGH");
          }
        }
      }
    }
```

Symbols verified to exist: `hitResult.hasHit`, `hitResult.value`, `bMask`,
`COLL_APPLYVICTIMSTATE` (=16), `COLL_DOEVERYTHING` (=1), `PERC_ASSESSOTHERSDAMAGE` (=9),
`owner.sendPassivePerc`, `isUnconscious`, `isDead`, `emitSoundSVM` (all in npc.cpp /
constants.h). The `bMask` gate is retained (conservative; the original has no mask gate but
for melee `bMask==COLL_DOEVERYTHING` and `hasHit==true`, so behaviour is unchanged there);
the only behavioural change is decoupling the witness perception from `value>0`.
