# DAM_BARRIER deep-water instant-kill missing in TouchDamage

**Confidence:** High

## Original function + address

`oCNpc::OnDamage_Hit` (Gothic2.exe `0x00666610`). Near the end of the routine,
after the per-type protection has been summed and the effective damage stored in
the descriptor, the code tests the damage-mode mask for the **DAM_BARRIER** bit
(`HasFlag(self, damageMode, 0x1)`). If that bit is set, it then queries the
victim's animation controller water level (`oCAniCtrl_Human::GetWaterLevel`). When
the water level is **greater than 1** (i.e. the NPC is swimming/diving, fully in the
water column), the effective/real damage is **overwritten with the victim's current
HP** (`GetAttribute(self, ATR_HITPOINTS)`), producing an instant kill. This is the
classic "magic barrier in deep water drowns you" behaviour: walking into a barrier
on land deals normal trigger damage, but swimming into it kills outright.

All special-case death/immortal/godmode handling then runs once on this single
descriptor (the touch-damage event is delivered to `OnDamage` as one `oCMsgDamage`
carrying the combined damage-type mask, address `0x00615b70`/`0x00615c70`).

## OpenGothic file:line

`game/world/triggers/touchdamage.cpp:51-55` (the per-type loop) and the helper
`TouchDamage::takeDamage` at `game/world/triggers/touchdamage.cpp:70-74`.

OpenGothic re-implements the damage application directly instead of routing the
touch event through the NPC damage pipeline. It loops over each active damage type
and applies `max(damage - protection[i], 0)` via `changeAttribute`. There is **no
handling of the DAM_BARRIER bit at all**: the `barrier` type is treated like any
other, dealing only the trigger's flat `damage` (minus the BARRIER protection slot).

## Divergence

In the original, a swimming NPC (water level > 1) that touches a DAM_BARRIER touch
trigger dies instantly (damage forced to full HP). In OpenGothic the same NPC takes
only the small flat trigger damage. Gameplay-different for the invisible magic
barriers placed over deep water in the world.

## Proposed patch

```cpp
// game/world/triggers/touchdamage.cpp  (TouchDamage::tick, inside the per-npc loop)

// OLD
    auto& hnpc = npc->handle();
    for(size_t i=0; i<zenkit::DamageType::NUM; ++i) {
      if(!mask[i])
        continue;
      takeDamage(*npc,int32_t(damage),hnpc.protection[i]);
      }

// NEW
    auto& hnpc = npc->handle();
    // NOTE: in original-game oCNpc::OnDamage_Hit (Gothic2.exe 0x00666610) a DAM_BARRIER
    // hit on a victim whose water level > 1 (swimming/diving) overrides the damage with the
    // victim's full current HP, i.e. an instant kill. Land hits keep the normal flat damage.
    if(mask[zenkit::DamageType::BARRIER] && npc->isSwim()) {
      npc->changeAttribute(ATR_HITPOINTS,-npc->attribute(ATR_HITPOINTS),false);
      continue;
      }
    for(size_t i=0; i<zenkit::DamageType::NUM; ++i) {
      if(!mask[i])
        continue;
      takeDamage(*npc,int32_t(damage),hnpc.protection[i]);
      }
```

`isSwim()` (`game/world/objects/npc.h:196`) corresponds to the original
`GetWaterLevel() > 1` (fully-submerged / swimming) state.
