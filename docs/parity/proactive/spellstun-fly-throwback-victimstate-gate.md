# FLY (knockback) throwback fires without the spell's victim-state collision flag

**Confidence:** High

## Original function + address
`oCNpc::OnDamage_Anim` (Gothic2.exe `0x00675bd0`), reached from the spell/weapon
damage pipeline `oCNpc::OnDamage` (`0x006660e0`) → `OnDamage_Hit` (`0x00666610`) →
`OnDamage_Anim`. Inside `OnDamage_Anim` the engine computes a local predicate (call it
`applyVictimState`) at the top of the function:

    applyVictimState = ( (collisionMask & COLL_APPLYVICTIMSTATE/0x10) != 0
                         || (collisionMask & COLL_DOEVERYTHING/0x01) != 0 )
                       && (flyForce > 0)

where `flyForce` is the protection-adjusted BARRIER+FLY damage (descriptor slots
`+0xa8`+`+0xb8`, the same value later handed to `oCAIHuman::StartFlyDamage`). The actual
throwback branch is taken only when:

    if (applyVictimState == 0 || GetWaterLevel() != 0 || GetBodyState() == BS_LIE/0xc)
        // no throwback (facial reaction only)
    else
        oCAIHuman::StartFlyDamage(force = damageDescriptor[+0xa8] + damageDescriptor[+0xb8], dir)

So in the original a FLY-type hit only knocks the victim back when the resolved collision
mask carries the victim-state bit (`COLL_APPLYVICTIMSTATE` or the catch-all
`COLL_DOEVERYTHING`) **and** the knockback force is non-zero, and never while the target is
in water or lying down. A spell whose `C_CanNpcCollideWithSpell` mask omits the victim-state
bit (e.g. `COLL_APPLYDAMAGE` alone, or `COLL_DONOTHING`) deals its damage with no throwback.

## OpenGothic file:line
`game/world/objects/npc.cpp:2170-2173` (in `Npc::takeDamage(Npc&, const Bullet*, CollideMask, int32_t, bool)`).

## Divergence
OpenGothic triggers the throwback on `hitResult.hasHit` alone:

    if(hitResult.hasHit && (damageType & (1<<zenkit::DamageType::FLY)))
        mvAlgo.accessDamFly(...);

`hitResult.hasHit` is set `true` even when the collision mask contributes no damage:
`DamageCalculator::damageValue` returns `Val(0,true,true)` whenever the mask lacks
`COLL_APPLYDAMAGE|…|COLL_DOEVERYTHING` (damagecalculator.cpp:119-120). Combined with the
fact that `Npc::takeDamage` (npc.cpp:2103-2106) calls into the damage path *even for*
`bMask==COLL_DONOTHING`, the consequence is that a FLY-type spell knocks the target back
regardless of what `C_CanNpcCollideWithSpell` returned — including masks that grant no
victim state (`COLL_APPLYDAMAGE` only) or that say to do nothing at all (`COLL_DONOTHING`).
The original gates the throwback on the victim-state bit.

Note that OpenGothic *already* gates the adjacent PERC_ASSESSOTHERSDAMAGE /
DEFEAT/MURDER/AARGH reactions on `(bMask&(COLL_APPLYVICTIMSTATE|COLL_DOEVERYTHING))`
(npc.cpp:2180, 2187); the FLY throwback was simply left out of that same gate, so the fix
restores internal consistency. The melee path is unaffected: it enters `takeDamage` with
`bMask==COLL_DOEVERYTHING` (npc.cpp:2087), which satisfies the gate.

## Proposed patch
`game/world/objects/npc.cpp`

OLD:
```cpp
  // throw enemy
  if(hitResult.hasHit && (damageType & (1<<zenkit::DamageType::FLY))) {
    mvAlgo.accessDamFly(x-other.x, z-other.z, lastHitType);
    }
```

NEW:
```cpp
  // throw enemy
  // NOTE: in original-game oCNpc::OnDamage_Anim (Gothic2.exe 0x00675bd0) the FLY throwback
  // (oCAIHuman::StartFlyDamage) is gated on applyVictimState = (mask has COLL_APPLYVICTIMSTATE
  // or COLL_DOEVERYTHING) && knockback-force>0; a spell whose C_CanNpcCollideWithSpell mask
  // omits the victim-state bit (incl. COLL_DONOTHING / COLL_APPLYDAMAGE-only) must not knock
  // the target back. hitResult.hasHit is true even for value==0 / no-victim-state masks, so
  // gate on the same flag already used for the perception broadcasts below.
  if(hitResult.hasHit && (damageType & (1<<zenkit::DamageType::FLY)) &&
     (bMask&(COLL_APPLYVICTIMSTATE|COLL_DOEVERYTHING))) {
    mvAlgo.accessDamFly(x-other.x, z-other.z, lastHitType);
    }
```

Symbols grep-verified to exist: `COLL_APPLYVICTIMSTATE`, `COLL_DOEVERYTHING`
(game/game/constants.h:264,268); `bMask`, `hitResult.hasHit`, `damageType`,
`mvAlgo.accessDamFly` (npc.cpp:2109,2145,2120,2172).

### Related but DEFERRED
The stumble/interrupt reaction (npc.cpp:2154-2167) and the constant `0.75f` knockback
magnitude in `MoveAlgo::accessDamFly` (movealgo.cpp:512) also differ from the original
(original scales `StartFlyDamage` by the BARRIER+FLY force and uses a more complex
weapon-mode/interrupt branch). These are deferred: the `OnDamage_Anim` branch selecting
stumble vs. throwback is heavily obscured by debug-string noise in the decompile and its
gating could not be reduced to a high-confidence one-liner without risking a regression.
