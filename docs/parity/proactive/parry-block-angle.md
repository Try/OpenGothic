# Parry/block resolution uses a 30-degree cone instead of 90

**Confidence:** High

## Original function

`oCAniCtrl_Human::CanParade(oCNpc*)` (Gothic2.exe `0x006b15b0`, oAniCtrl.cpp),
called as the block decision at strike time from
`oCAniCtrl_Human::HitCombo` (`0x006b0260`).

When the attacker's combo reaches its hit frame, `HitCombo` gates a real hit on:
`IsInFightRange(me,target)` && `IsInFightFocus(me,target)` (attacker faces
target) && `IsSameHeight(...)`, and the victim not being FLY-immune. It then
calls `CanParade(target->GetAnictrl(), me)`. If `CanParade` returns non-zero the
strike is converted to a parade (`StartParadeEffects`) and **no hit is created**;
otherwise `CreateHit` runs.

Inside `CanParade` the only angular gate on the **defender** is:
`GetAngles(defenderNpc, attackerPos, &yaw, &pitch)` then
`if (abs(yaw) > 90) return 0;` (the literal `0x5a` = 90 degrees). So the
defender parries an incoming strike as long as the attacker is within a WIDE
+/-90-degree front cone of the defender's facing (plus a parade animation being
active and the weapon-mode/JUMP rules). It is NOT the narrow 30-degree
fight-focus cone used for landing attacks.

## OpenGothic

`game/world/objects/npc.cpp:2040-2042`

```
const bool  isBlock = (!other.isMonster() || other.inventory().activeWeapon()!=nullptr) &&
                       fghAlgo.isInFocusAngle(*this,other) &&
                       pose.isDefence(owner.tickCount());
```

`fghAlgo.isInFocusAngle(*this,other)` is the no-argument overload
(`game/game/fightalgo.cpp:336-339`), a **30-degree** cone (`cos(30deg)`),
measuring the defender (`*this`) facing toward the attacker (`other`).

## Divergence

The defender's block angle is a 30-degree cone in OG but a 90-degree-yaw cone
in the original. An NPC that is in its parade/defence window but whose attacker
is ~30-90 degrees off its facing will, in the original, still block the hit
(`CanParade` returns 1), but in OG `isBlock` is false and the strike applies
full damage via `takeDamage(...COLL_DOEVERYTHING...)`. Gameplay-different:
side/off-axis attacks against a parrying NPC are unblockable in OG, whereas the
original blocks anything inside the front half-circle. This is the actual
hit-resolution path; the separately-tracked `fai-prehit-react-angle` finding
fixes the AI *reaction selection* path (`FindNextFightAction`), not this block
decision.

`angleTest` is yaw-only, so the faithful analogue of the 0x5a gate is
`isInFocusAngle(npc,tg,90)`, matching the existing `isInJumpBackAngle` (90deg)
already used one line above for the jump-back case.

## Proposed patch

`game/world/objects/npc.cpp`

OLD:
```
  const bool  isBlock = (!other.isMonster() || other.inventory().activeWeapon()!=nullptr) &&
                         fghAlgo.isInFocusAngle(*this,other) &&
                         pose.isDefence(owner.tickCount());
```
NEW:
```
  // NOTE: in original-game oCAniCtrl_Human::CanParade (Gothic2.exe 0x006b15b0,
  // called from HitCombo 0x006b0260) the only angular gate on the defender is a
  // WIDE +/-90-degree front cone (GetAngles yaw, 0x5a), not the 30-degree
  // fight-focus cone used for landing attacks. angleTest is yaw-only, so use 90.
  const bool  isBlock = (!other.isMonster() || other.inventory().activeWeapon()!=nullptr) &&
                         fghAlgo.isInFocusAngle(*this,other,90) &&
                         pose.isDefence(owner.tickCount());
```
