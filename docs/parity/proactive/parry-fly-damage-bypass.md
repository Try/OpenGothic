# Parry resolution: FLY-type (knockback) attacks must bypass parade entirely

**Confidence:** Medium-High (code divergence is unambiguous; vanilla trigger frequency depends on which attacks carry DAM_FLY).

## Original function + address

In `oCAniCtrl_Human::HitCombo` (Gothic2.exe `0x006b0260`), when the attacker reaches its
hit frame and the target is in fight range / focus / same-height, the engine resolves the strike
as follows (prose): it first calls `oCNpc::GetDamageByType(attacker, 0x10)` — `0x10` is the
`DAM_FLY` bit (damage slot index 4, `oCNpc+0x21c`, confirmed by decompiling
`oCNpc::GetDamageByType` @ `0x0072fdb0`). **Only when that FLY-damage value is zero** does it go
on to consult the defender's parade ability via `CanParade` (`oCAniCtrl_Human::CanParade`
@ `0x006b15b0`) and, on success, run `StartParadeEffects` and deal no damage. If the attacker
deals any FLY damage, the whole parade/jump-dodge branch is skipped and `CreateHit` is called
unconditionally. Net rule: **a knockback (FLY) attack can be neither parried nor jump-dodged.**
`CanParade` itself is the single gate covering both the parade pose and the jump-back dodge
(it scans the active anim name for `PARADE`, then specially handles the `JUMP` variants), so the
FLY pre-gate suppresses both forms of damage avoidance.

## OpenGothic file:line

`game/world/objects/npc.cpp:2067-2086` — `Npc::takeDamage(Npc& other, const Bullet* b)`.

## Divergence

OpenGothic computes the damage-avoidance decision (`isBlock` / `isJumpb`) from the defender's
pose and front cone only. It never inspects whether the attacker's strike carries FLY damage,
so a defender holding a parade (or a jump-back) negates **both** the damage **and** the knockback
of a FLY-type attack. In the original, such attacks are unblockable and always land
(damage + throwback applied via the `FLY` path that OG already implements at
`npc.cpp:2152/2158`). This is the resolution-logic piece (`GetDamageByType(attacker, FLY)==0`
guarding the parade call) that was not ported when the 90-degree cone and monster rules were
moved into `isBlock`.

## Proposed patch

Grep-verified OG symbols: `DamageCalculator::damageTypeMask(Npc&)` (already used at
`npc.cpp:2115`), `zenkit::DamageType::FLY` (already used at `npc.cpp:2152`), include
`game/damagecalculator.h` (present, `npc.cpp:8`). `other` is a non-const `Npc&` here, matching
the `damageTypeMask` signature.

OLD (`game/world/objects/npc.cpp`):
```cpp
  const bool  isBlock = (!other.isMonster() || other.inventory().activeWeapon()!=nullptr) &&
                         fghAlgo.isInFocusAngle(*this,other,90) &&
                         pose.isDefence(owner.tickCount());

  lastHit = &other;
  if(!isPlayer())
    setOther(&other);
  owner.sendPassivePerc(*this,other,*this,PERC_ASSESSFIGHTSOUND);

  if(!(isBlock || isJumpb) || b!=nullptr) {
```

NEW:
```cpp
  const bool  isBlock = (!other.isMonster() || other.inventory().activeWeapon()!=nullptr) &&
                         fghAlgo.isInFocusAngle(*this,other,90) &&
                         pose.isDefence(owner.tickCount());
  // NOTE: in original-game oCAniCtrl_Human::HitCombo (Gothic2.exe 0x006b0260) the parade path is
  // only reached when oCNpc::GetDamageByType(attacker, DAM_FLY/0x10) == 0; a FLY (knockback)
  // attack skips CanParade entirely and can be neither parried nor jump-dodged. OpenGothic omitted
  // this gate, so a defender could block FLY-type attacks.
  const bool  flyAtk  = (DamageCalculator::damageTypeMask(other) & (1<<zenkit::DamageType::FLY))!=0;

  lastHit = &other;
  if(!isPlayer())
    setOther(&other);
  owner.sendPassivePerc(*this,other,*this,PERC_ASSESSFIGHTSOUND);

  if(!(isBlock || isJumpb) || b!=nullptr || flyAtk) {
```

Note: the original gates on the FLY damage *value* (`oCNpc+0x21c`); OG's `damageTypeMask` returns
the active weapon's (or NPC's) `damage_type` bitmask, which carries the FLY bit precisely when
that slot is non-zero, so the mask test is the faithful and minimal analog.
