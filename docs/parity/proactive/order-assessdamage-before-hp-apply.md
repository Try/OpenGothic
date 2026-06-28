# Order divergence: self PERC_ASSESSDAMAGE runs before HP is applied in takeDamage

**Confidence:** Medium-High

## Original fn + address (prose)

`oCNpc::OnDamage` (Gothic2.exe `0x006660e0`) drives a fixed sequence of helper
calls. After the conversation/condition early-outs it runs, in this order:

1. `oCNpc::OnDamage_Hit` (`0x00666610`) — computes the final, protection/minimal-damage-clamped
   damage value and **applies it immediately**: near the tail it calls the hit-points
   `ChangeAttribute` helper (`FUN_0072ff60(0, -damage)`), so the victim's HP is already
   decremented when this returns.
2. `oCNpc::OnDamage_Condition` (`0x0066cf30`) — decides death / knock-out and sets the
   descriptor death bit (`oSDamageDescriptor+0x90 & 4`).
3. (if the valid-hit bit `+0x90 & 1` is set) `OnDamage_Anim` (`0x00675bd0`, stumble/FLY throwback),
   `OnDamage_Effects_Start`, then `OnDamage_Script` (`0x0066e220`).
4. `OnDamage_Script` calls `oCNpc::AssessDamage_S` (`0x0075c280`) **only when the death bit is
   clear**. `AssessDamage_S` walks the victim's perception table for entry type 8
   (PERC_ASSESSDAMAGE) and `StartAIState`s the victim's own assess-damage handler (its
   `B_AssessDamage`), then broadcasts `CreatePassivePerception(this, 9, attacker, this)` =
   PERC_ASSESSOTHERSDAMAGE to witnesses.

So the original runs the victim's **PERC_ASSESSDAMAGE AI after the HP loss has been applied and
after death has been decided** (and never on a lethal blow).

## OG file:line

`/Users/admin/Downloads/opengothic/game/world/objects/npc.cpp`

- Self PERC_ASSESSDAMAGE fired at the very **top** of `takeDamage`, line 2221:
  ```
  if(!isSpell || splCat==SpellCategory::SPELL_BAD) {
    perceptionProcess(other,this,0,PERC_ASSESSDAMAGE);   // <-- runs B_AssessDamage AI now
    fghAlgo.onTakeHit();
    implFaiWait(0);
    }
  ```
- HP is applied much later via `changeAttribute(ATR_HITPOINTS,-hitResult.value,dontKill)` at line 2281.
- The sibling witness broadcast (`PERC_ASSESSOTHERSDAMAGE`) was already relocated to the
  post-damage, death-gated site at lines 2311-2312.

`perceptionProcess` (npc.cpp:4695) invokes the handler **synchronously**
(`owner.script().invokeState(this,&pl,victim,perception[perc].func)`), so the victim's
`B_AssessDamage` Daedalus code reads `self.attribute[ATR_HITPOINTS]` at the moment of the call.

## Divergence

Same two operations — *apply the hit's HP loss* and *run the victim's assess-damage AI* — are
ordered oppositely:

- **Original:** apply HP (OnDamage_Hit) -> decide death (OnDamage_Condition) -> run assess-damage
  AI (OnDamage_Script -> AssessDamage_S), and only if the blow was not lethal.
- **OpenGothic:** run assess-damage AI **first** (line 2221), then later compute the value,
  set the stumble anim, and finally apply HP (line 2281).

Observable effects, all from the same misplacement:
1. The victim's `B_AssessDamage` reads **pre-hit HP**. Standard scripts gate flee / call-for-help
   on an HP threshold, so an NPC straddling that threshold reacts to the wrong HP.
2. OG fires self ASSESSDAMAGE even on a **lethal** blow (it runs before HP/death is known); the
   original suppresses it (death bit set) and lets `DoDie` send ASSESSMURDER instead. The witness
   half was already fixed for exactly this reason; the self half was missed.
3. OG fires self ASSESSDAMAGE even on a **re-hit of an already-downed** NPC (line 2221 runs before
   the `isDown()` early-returns at 2230-2245), which the original's death/KO gating prevents.

This is the documented-but-half-applied sibling of the existing OTHERSDAMAGE-below-death fix:
the in-file NOTE at npc.cpp:2301-2310 already states `AssessDamage_S` fires *both* the self
ASSESSDAMAGE and the witness OTHERSDAMAGE, post-damage and death-gated, yet only the witness call
was moved. (Distinct from the excluded OTHERSDAMAGE-below-death fix, which concerns perc 9.)

## Proposed patch

Move the self `perceptionProcess(...,PERC_ASSESSDAMAGE)` out of the top block and fold it into the
already-relocated post-damage, death-gated site next to the witness broadcast, matching the single
`AssessDamage_S` call site (self StartAIState first, then the perc-9 broadcast). Leave
`fghAlgo.onTakeHit()` / `implFaiWait(0)` where they are (fight bookkeeping, HP-independent).

OLD (npc.cpp ~2220):
```cpp
  if(!isSpell || splCat==SpellCategory::SPELL_BAD) {
    perceptionProcess(other,this,0,PERC_ASSESSDAMAGE);
    fghAlgo.onTakeHit();
    implFaiWait(0);
    }
```
NEW:
```cpp
  if(!isSpell || splCat==SpellCategory::SPELL_BAD) {
    fghAlgo.onTakeHit();
    implFaiWait(0);
    }
```

OLD (npc.cpp ~2311):
```cpp
  if(hitResult.hasHit && (bMask&(COLL_APPLYVICTIMSTATE|COLL_DOEVERYTHING)) && !isDead())
    owner.sendPassivePerc(*this,other,*this,PERC_ASSESSOTHERSDAMAGE);
```
NEW:
```cpp
  // NOTE: in original-game oCNpc::OnDamage @0x006660e0 the HP loss is applied first
  // (OnDamage_Hit @0x00666610 calls ChangeAttribute), THEN OnDamage_Script @0x0066e220 ->
  // AssessDamage_S @0x0075c280 runs the victim's own PERC_ASSESSDAMAGE handler and broadcasts
  // PERC_ASSESSOTHERSDAMAGE, and only when the death bit (oSDamageDescriptor+0x90 & 4) is clear.
  // OpenGothic ran the self PERC_ASSESSDAMAGE at the top of takeDamage, before changeAttribute,
  // so B_AssessDamage read pre-hit HP, and it fired even on a lethal blow / re-hit of a downed
  // NPC. Run it here (post-damage, death-gated) alongside the witness broadcast, mirroring the
  // single AssessDamage_S call site (self assess first, then the perc-9 broadcast).
  if(hitResult.hasHit && (bMask&(COLL_APPLYVICTIMSTATE|COLL_DOEVERYTHING)) && !isDead()) {
    perceptionProcess(other,this,0,PERC_ASSESSDAMAGE);
    owner.sendPassivePerc(*this,other,*this,PERC_ASSESSOTHERSDAMAGE);
    }
```

Note: the original's `AssessDamage_S` is gated on the victim-state mask bit + death, not on spell
category, and the OG witness broadcast at this site is already un-spell-category-gated; folding the
self call in here matches that. If a tighter, behavior-minimal change is preferred, the moved
`perceptionProcess` can keep the `(!isSpell || splCat==SPELL_BAD)` guard, but the unguarded form is
the faithful match to `AssessDamage_S`.
