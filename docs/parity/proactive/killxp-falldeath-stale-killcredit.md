# Fall-death wrongly credits the last attacker (stale `lastHit`) for the kill / death-XP

**Confidence:** High

## Original function + address

`oCNpc::CreateFallDamage(float)` @ `0x00681da0` builds an `oSDamageDescriptor` and
wraps it in an `oCMsgDamage` whose **sender/origin argument is `0` (null)** —
`oCMsgDamage(msg, /*sender=*/0, descriptor)` — before posting it through the vob's
`zCEventManager`. When that message is consumed, `oCNpc::OnDamage_Script` @ `0x00813xxx`
runs the script-side assessment as `oCNpc::AssessDamage_S(self, *(oCNpc**)(descr+8), value)`,
i.e. `other` is taken from the damage descriptor's origin field, which for a fall is the
null message sender. So a fatal fall sets `self.other = NULL` for the resulting
`PERC_ASSESSDAMAGE` / `ZS_Dead` transition. With `other == NULL`, the Daedalus death-XP
path (`ZS_Dead` → `B_GiveDeathXP`, which only rewards the hero when the murderer is the
player) awards **no kill credit and no XP**. The same null-origin convention is what the
engine uses for all self-inflicted environmental damage (the OG dive/drown tick already
mirrors it by clearing `lastHit`).

## OG file:line

`/Users/admin/Downloads/opengothic/game/world/objects/npc.cpp:2258` — `Npc::takeFallDamage`,
specifically the lethal path at line `2286` (`changeAttribute(ATR_HITPOINTS,-dmg.value,false)`).

## Divergence

OpenGothic routes the kill-credit / `ZS_Dead` `other` through `lastHit`: `onNoHealth`
(npc.cpp:594) does `setOther(lastHit)` and the started state is invoked with `currentOther`.
`lastHit` is written only by the combat damage paths (npc.cpp:2104, 2133) and the dive/drown
tick (npc.cpp:2503, where it is deliberately set to `nullptr`); it is read only at
npc.cpp:594. `Npc::takeFallDamage` never touches `lastHit`. Therefore an NPC the player (or
another NPC) hit at some earlier point, that then runs off a ledge and dies from the fall,
keeps the **stale `lastHit` = previous attacker**. `onNoHealth` sets `other` to that stale
attacker, so `ZS_Dead`/`B_GiveDeathXP` wrongly credits the kill and grants the hero death-XP
for a death the engine attributes to nobody. (The dive-death path is already correct; only
the fall path was missed.)

`lastHit` feeds nothing but kill credit (sole read at npc.cpp:594), so clearing it on the
fall hit is safe: a non-lethal fall that is later finished by a real attacker re-sets
`lastHit` on that attacker's hit before death, and `lastHitType` — used for the
DeadA/DeadB fall/death animation — is a separate field set independently at npc.cpp:2267-2268.

## Proposed patch

`game/world/objects/npc.cpp`, in `Npc::takeFallDamage`, immediately before the
`changeAttribute` that can drop HP to 0:

OLD:
```cpp
  int32_t hp = attribute(ATR_HITPOINTS);
  if(hp>dmg.value) {
    emitSoundSVM("SVM_%d_AARGH");
    clearState(true);
    }
  changeAttribute(ATR_HITPOINTS,-dmg.value,false);
  }
```

NEW:
```cpp
  int32_t hp = attribute(ATR_HITPOINTS);
  if(hp>dmg.value) {
    emitSoundSVM("SVM_%d_AARGH");
    clearState(true);
    }
  // NOTE: in original-game oCNpc::CreateFallDamage @0x00681da0 the fall oCMsgDamage is posted
  // with a null sender (origin), so oCNpc::OnDamage_Script -> AssessDamage_S receives other=NULL
  // and a fatal fall has no killer -> ZS_Dead/B_GiveDeathXP awards no kill credit/XP. OpenGothic
  // routes kill credit through lastHit (onNoHealth -> setOther(lastHit)) but never cleared it on
  // the fall path, so a stale previous attacker (e.g. the hero) was wrongly credited with the
  // kill and given death-XP. Mirror the dive/drown tick (npc.cpp:2503) and drop the attacker.
  lastHit = nullptr;
  changeAttribute(ATR_HITPOINTS,-dmg.value,false);
  }
```
