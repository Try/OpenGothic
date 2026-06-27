# Finishing-move kill is not attributed to the finisher

**Confidence:** Medium

## Original function + address (prose only)

The script external `AI_FinishingMove` (handler `FUN_006f5a40` in `oGameExternal.cpp`)
does not itself kill anything; it only enqueues an `oCMsgAttack` of sub-type 4 carrying
the `T_<weaponhit>SFINISH` animation id and `msg->target = <victim>`, addressed to the
finisher NPC. The kill is performed by `oCNpc::EV_AttackFinish` (Gothic2.exe @0x00751af0,
`oNpc.cpp`). That handler runs the finish animation and, only once
`zCModel::GetProgressPercent(finishAni) >= 0.5` **and** the victim is still in the
unconscious state (`oCNpc_States::IsInState(victim, -4)`), deals the lethal blow by
calling the victim's virtual damage entry `victim->OnDamage(<finisher>, ...)` (the virtual
`oCNpc::OnDamage(zCVob*,zCVob*,float,int,zVEC3 const&)` @0x0067b860), followed by
`zCAIPlayer::AddBlood` and `oCNpc::OnDamage_Sound`. Because the killing damage is routed
through `OnDamage` with the **finisher** as the damage source, the victim's death state
(and therefore the kill/XP credit, i.e. the `other` seen by `ZS_Dead`) is attributed to
the NPC that performed the finishing move.

## OpenGothic file:line

`game/world/objects/npc.cpp:3953` — `Npc::finishingMove()`

```
if(doAttack(Anim::AttackFinish,BS_HIT)) {
  currentTarget->hnpc->attribute[ATR_HITPOINTS] = 0;
  currentTarget->checkHealth(true,false);
  owner.sendPassivePerc(*this,*this,*currentTarget,PERC_ASSESSMURDER);
  return true;
  }
```

## Divergence

OpenGothic kills the victim by writing `ATR_HITPOINTS = 0` directly and calling
`checkHealth(true,false)`. It never records the finisher as the killer. The death then
flows through `Npc::onNoHealth` (`npc.cpp:584`), whose only attribution is
`setOther(lastHit)` (line 592). `lastHit`/`setOther` are normally written in the regular
damage path (`perceiveAttack` `npc.cpp:2081-2083`, `takeDamage` `npc.cpp:2099-2101`), but
`finishingMove()` skips them — the normal damage path is unreachable for a downed target
anyway because `takeDamage()` early-returns on `isDown()` (`npc.cpp:2095`). Consequently
the death-state `other` is whoever last damaged the victim *before* it went down, not the
finisher. When the finisher is also the last damager (player downs and finishes the same
enemy) the result coincides with the original; when a different entity downed the victim
(an ally, a fall, etc.) and the player/NPC then finishes it, the kill credit is
mis-attributed, diverging from the original where the finishing `OnDamage` re-stamps the
finisher as the source.

(The murder/witness perception is already correctly attributed: OpenGothic sends
`PERC_ASSESSMURDER` from `*this`, the finisher. Only the death-state `other`/kill credit
is wrong.)

## Proposed patch

Mirror the attribution that the original's `OnDamage(<finisher>, ...)` performs (and that
OpenGothic's own `takeDamage`/`perceiveAttack` already do) by stamping the finisher as the
victim's last hitter before the lethal `checkHealth`.

OLD (`game/world/objects/npc.cpp`, `Npc::finishingMove`):
```cpp
  if(doAttack(Anim::AttackFinish,BS_HIT)) {
    currentTarget->hnpc->attribute[ATR_HITPOINTS] = 0;
    currentTarget->checkHealth(true,false);
    owner.sendPassivePerc(*this,*this,*currentTarget,PERC_ASSESSMURDER);
    return true;
    }
```

NEW:
```cpp
  if(doAttack(Anim::AttackFinish,BS_HIT)) {
    // NOTE: in original-game oCNpc::EV_AttackFinish @0x00751af0 the finishing blow is dealt
    // via victim->OnDamage(<finisher>,...), so the death is credited to the finisher; record
    // the finisher as lastHit/other here so onNoHealth's setOther(lastHit) matches.
    currentTarget->lastHit = this;
    if(!currentTarget->isPlayer())
      currentTarget->setOther(this);
    currentTarget->hnpc->attribute[ATR_HITPOINTS] = 0;
    currentTarget->checkHealth(true,false);
    owner.sendPassivePerc(*this,*this,*currentTarget,PERC_ASSESSMURDER);
    return true;
    }
```

Grep-verified OG symbols: `Npc::lastHit` (`npc.h:587`, written by `perceiveAttack`/`takeDamage`),
`Npc::setOther` (`npc.h:402`, def `npc.cpp:3158`), `Npc::isPlayer` (`npc.h`/`npc.cpp:548`),
`Npc::checkHealth` (`npc.cpp:558`) which routes to `onNoHealth`→`setOther(lastHit)`
(`npc.cpp:592`). `finishingMove` is an `Npc` method, so `currentTarget`'s private
`lastHit` member is accessible.

Reason for Medium (not High): the impact is confined to the case where the finisher is not
the entity that last damaged the (now-downed) victim; in the common player-downs-and-finishes
path OpenGothic already coincides with the original. The original-side routing of the
finishing blow through `OnDamage(<finisher>)` is inferred from the virtual-dispatch call at
the finish-ani 50% mark in `EV_AttackFinish` (target->vtbl OnDamage with the finisher as
source) plus the adjacent `AddBlood`/`OnDamage_Sound` calls.
