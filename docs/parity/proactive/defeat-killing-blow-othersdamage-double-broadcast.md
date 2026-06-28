# Killing blow double-broadcasts PERC_ASSESSOTHERSDAMAGE alongside ASSESSMURDER

**Confidence:** Medium-High (divergence existence: high, verified end-to-end in the original damage
pipeline; exact faithful patch: medium — it relocates one already-present broadcast statement and
gates it on the post-damage death result).

## Original function + address (prose)

The original routes every registered hit through `oCNpc::OnDamage` (Gothic2.exe `@0x006660e0`),
which calls its sub-stages in a fixed order:

1. `oCNpc::OnDamage_Hit` (`@0x0066???`) — applies the HP loss.
2. `oCNpc::OnDamage_Condition` (`@0x0066cf30`) — decides the outcome and sets two result bits on the
   damage descriptor (`oSDamageDescriptor + 0x90`): the **death** bit `0x4` and the **unconscious**
   bit `0x8`. The death bit is set only when the victim ends up dead (`IsDead`) **and** the
   unconscious bit was *not* set (the knockout path is gated on `C_DropUnconscious`, an attacker
   existing, and not-in-water).
3. `oCNpc::OnDamage_Script` (`@0x0066e220`) — its tail calls `oCNpc::AssessDamage_S`
   (`@0x0075c280`) **but only when the death bit is clear**: `if ((descr[0x90] & 4) == 0) { ...
   AssessDamage_S(victim, attacker, value); }`. `AssessDamage_S` does two things: it runs the
   victim's own `PERC_ASSESSDAMAGE` (perc 8) state, and then unconditionally broadcasts
   `oCNpc::CreatePassivePerception(PERC_ASSESSOTHERSDAMAGE = 9, other=attacker, victim=this)` — the
   witness/comrade alert.
4. `oCNpc::OnDamage_Events` (`@0x0067abe0`) — runs `oCNpc::DropUnconscious` (`@0x00735eb0`, broadcasts
   `PERC_ASSESSDEFEAT = 7`) when the unconscious bit is set and `oCNpc::DoDie` (`@0x00736760`,
   broadcasts `PERC_ASSESSMURDER = 6` when an attacker exists) when the death bit is set.

Net witness broadcasts in the original, per blow:

| outcome | self ASSESSDAMAGE | witness OTHERSDAMAGE | witness DEFEAT/MURDER |
|---|---|---|---|
| absorbed (value==0) / alive | yes | yes | — |
| knockout (bit 8) | yes | yes | DEFEAT |
| **kill (bit 4)** | **no** | **no** | **MURDER only** |

The death bit uniquely suppresses both `AssessDamage_S` effects; the knockout bit does not. So a
**killing blow alerts nearby NPCs with ASSESSMURDER only**, never ASSESSOTHERSDAMAGE.

## OG file:line

`game/world/objects/npc.cpp:2267-2268` (inside `Npc::takeDamage`):

```cpp
if(hitResult.hasHit && (bMask&(COLL_APPLYVICTIMSTATE|COLL_DOEVERYTHING)))
  owner.sendPassivePerc(*this,other,*this,PERC_ASSESSOTHERSDAMAGE);
```

This fires *before* `changeAttribute` (line 2272) and is gated only on `hasHit` + mask — never on
whether the blow turns out to be fatal. The DEFEAT/MURDER block at 2274-2289 then fires *in addition*.

## Divergence

On the killing blow OpenGothic broadcasts **both** `PERC_ASSESSOTHERSDAMAGE` (line 2268) **and**
`PERC_ASSESSMURDER` (line 2279) to the same set of witnesses, whereas the original fires only
`PERC_ASSESSMURDER`. Nearby allies therefore run *both* `B_AssessOthersDamage` ("someone got hurt,
help / aggro") and `B_AssessMurder` ("someone was murdered") on a single kill, double-processing the
event (e.g. duplicate target acquisition / aggression escalation / guild-attitude reactions). The
absorbed-hit (value==0) and knockout cases are unaffected — those still legitimately fire
OTHERSDAMAGE in the original, so the fix must preserve them.

(Related but masked: the victim's own `PERC_ASSESSDAMAGE` at npc.cpp:2204 is likewise suppressed on
a fatal blow in the original. In OG it still runs, but the immediate `onNoHealth` → `ZS_Dead` clears
the AI queue the same frame, so its effect is largely masked; left out of this surgical patch.)

## Proposed patch

Relocate the OTHERSDAMAGE broadcast below the death decision and gate it on the post-damage
`!isDead()` result, mirroring the original's death-bit suppression. Absorbed (never enters the
`value>0` block, so not dead → fires), knockout (`isUnconscious()`, not dead → fires), and alive hits
keep firing; only a kill is suppressed. The DEFEAT/MURDER/OTHERSDAMAGE broadcasts are all queued
passive percs (`sndPerc`), so the relative reorder is behaviorally inert.

```cpp
// OLD (npc.cpp:2262-2268)
  // NOTE: in original-game oCNpc::AssessDamage_S (Gothic2.exe 0x0075c280) the witness broadcast
  // CreatePassivePerception(PERC_ASSESSOTHERSDAMAGE) fires together with the self PERC_ASSESSDAMAGE
  // whenever the hit lands, with no dependency on the net damage value -- so a blow fully absorbed
  // by armour (value==0) still alerts nearby NPCs. OpenGothic nested it under value>0, suppressing
  // that witness reaction. The value-dependent DEFEAT/MURDER/AARGH reactions stay under value>0.
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
        // NOTE: ... AARGH (unchanged)
        emitSoundSVM("SVM_%d_AARGH");
        }
      }
    }

// NEW
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
        // NOTE: ... AARGH (unchanged)
        emitSoundSVM("SVM_%d_AARGH");
        }
      }
    }

  // NOTE: in original-game oCNpc::OnDamage @0x006660e0 calls oCNpc::OnDamage_Script @0x0066e220
  // (-> oCNpc::AssessDamage_S @0x0075c280, which fires the self PERC_ASSESSDAMAGE and the witness
  // PERC_ASSESSOTHERSDAMAGE broadcast) ONLY when the death bit (oSDamageDescriptor+0x90 & 4) is
  // clear. oCNpc::OnDamage_Condition @0x0066cf30 sets that bit on a lethal blow (victim dead and NOT
  // knocked unconscious), so a killing blow alerts witnesses with ASSESSMURDER only (from DoDie
  // @0x00736760), never ASSESSOTHERSDAMAGE. The knockout bit (8) does not suppress it, so a KO still
  // fires both DEFEAT and OTHERSDAMAGE. The armour-absorbed (value==0) hit never reaches death, so it
  // keeps firing. Gate the witness OTHERSDAMAGE on the post-damage !isDead() result so a kill no
  // longer double-broadcasts (OTHERSDAMAGE + MURDER) to nearby allies.
  if(hitResult.hasHit && (bMask&(COLL_APPLYVICTIMSTATE|COLL_DOEVERYTHING)) && !isDead())
    owner.sendPassivePerc(*this,other,*this,PERC_ASSESSOTHERSDAMAGE);
```

All symbols already in scope on these lines (`hitResult`, `bMask`, `COLL_APPLYVICTIMSTATE`,
`COLL_DOEVERYTHING`, `isDead()` @ npc.cpp:4303 / npc.h:283, `isUnconscious()`, `sendPassivePerc`,
`PERC_ASSESSOTHERSDAMAGE`/`PERC_ASSESSMURDER`/`PERC_ASSESSDEFEAT`). Single localized move + one
`&& !isDead()` guard; build-verifiable.
