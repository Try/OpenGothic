# Mob interaction is not aborted when an interruptable user takes a hit

**Confidence:** Medium-High

## Original function + address (prose)

In the original engine the damage-animation pipeline `oCNpc::OnDamage_Anim`
(Gothic2.exe @ 0x00678200) reacts to a non-fatal hit. After computing whether
the victim has a weapon drawn and whether it is mid-cast, it takes a dedicated
"GOTHIT/stumble" branch only when the npc has **no** weapon out, is **not**
casting, and `oCNpc::IsBodyStateInterruptable` (@ 0x0075efa0) returns true.
`IsBodyStateInterruptable` is `(bodystate & 0x3f80)==0 && (bodystate & 0x8000)!=0`,
i.e. none of the BS_MOD_* modifier bits (hidden/drunk/nuts/burning/controlled/
transformed) are set **and** the `BS_FLAG_INTERRUPTABLE` (0x8000) flag is set.

Inside that branch (call site @ 0x0067840a) the original first calls
`ClearEM`, then `oCNpc::Interrupt(this, 0, 0)` (@ 0x00735ab0), then
`SetBodyState(0x15)` (BS_STUMBLE) and plays the `T_<prefix>GOTHIT` animation.
The crucial side effect is `Interrupt`: because its first argument is 0, the
"detach the active mob" guard at the `interactMob` slot (`this+0x964`) fires
unconditionally and calls the mob's virtual `InterruptInteraction`
(`oCMobInter::InterruptInteraction` @ 0x00720ce0). That routine clears the
attach-position user, stops the interaction anim layer, puts back any interact
item, calls `SetInteractMob(0)`, resets the npc rotation and decrements the
mob's active-user count. Net result in the original: a hit on an npc/player who
is bound to an **interruptable** mob (the `BS_MOBINTERACT_INTERRUPT` /
`BS_ITEMINTERACT` schemes — forge, anvil, repair bench, item-use, etc.)
force-detaches them from the mob and plays the stumble.

## OpenGothic file:line

`game/world/objects/npc.cpp:2154-2168` (`Npc::takeDamage`):

```cpp
if(hitResult.hasHit) {
  auto state = bodyStateMasked();
  if(interactive()==nullptr && ((state&BS_FLAG_INTERRUPTABLE)!=BS_NONE || state==BS_RUN || state==BS_NONE)) {
    const bool noInter = (hnpc->bodystate_interruptable_override!=0);
    if(!noInter)
      visual.interrupt();
    if((damageType & (1<<zenkit::DamageType::FLY))==0)
      setAnimAngGet(lastHitType=='A' ? Anim::StumbleA  : Anim::StumbleB);
    }
  }
```

## Divergence

The whole interrupt/stumble block is gated behind `interactive()==nullptr`, so
when the victim is currently attached to a mob the block is skipped entirely and
**no detach ever happens** on a hit. Unlike death/unconsciousness
(`Npc::onNoHealth` -> `setInteraction(nullptr,true)` at npc.cpp:611) and the
TA/reset paths, a *survivable* interruptable hit leaves the npc/player physically
locked into an interruptable mob (e.g. standing at a forge / repair bench / item
interaction with `BS_MOBINTERACT_INTERRUPT`). In the original that same hit
yanks them out of the mob and stumbles. This is the engine-level
"abort-mob-interaction on combat/damage / interrupt-to-stand-state on
disturbance" behavior, and OpenGothic simply does not perform the detach.

(Secondary, lower-stakes note: `bodyStateMasked()` masks with
`BS_MAX | BS_FLAG_MASK`, dropping the BS_MOD_* bits, so OG's interruptable test
does not reproduce the original's extra `(bodystate & 0x3f80)==0` guard that
suppresses the stumble while burning/transformed/etc. Not mob-specific; called
out only for completeness.)

## Proposed patch (vs grep-verified OG symbols)

Grep-verified symbols: `interactive()` (npc.h:303), `Interactive::isLadder()`
(interactive.h:65), `setInteraction(Interactive*,bool)` (npc.h:304),
`BS_FLAG_INTERRUPTABLE`/`BS_RUN`/`BS_NONE` (game/game/constants.h).

OLD:
```cpp
  if(hitResult.hasHit) {
    auto state = bodyStateMasked();
    if(interactive()==nullptr && ((state&BS_FLAG_INTERRUPTABLE)!=BS_NONE || state==BS_RUN || state==BS_NONE)) {
      //NONE/RUN requires for monsters like waran
      const bool noInter = (hnpc->bodystate_interruptable_override!=0);
      if(!noInter) {
        //NOTE: kepp rotation animation: this results in more accurate fight with trolls
        // visual.setAnimRotate(*this,0);
        visual.interrupt(); // TODO: put down in pipeline, at Pose and merge with setAnimAngGet
        }

      if((damageType & (1<<zenkit::DamageType::FLY))==0)
        setAnimAngGet(lastHitType=='A' ? Anim::StumbleA  : Anim::StumbleB);
      }
    }
```

NEW:
```cpp
  if(hitResult.hasHit) {
    auto state = bodyStateMasked();
    if((state&BS_FLAG_INTERRUPTABLE)!=BS_NONE || state==BS_RUN || state==BS_NONE) {
      //NONE/RUN requires for monsters like waran
      // NOTE: in original-game oCNpc::OnDamage_Anim (Gothic2.exe 0x00678200) an
      // interruptable hit (IsBodyStateInterruptable 0x0075efa0) calls
      // oCNpc::Interrupt(0,0) (0x00735ab0), which force-detaches the active mob
      // via oCMobInter::InterruptInteraction (0x00720ce0) before the stumble.
      // Ladders are excluded here (OG special-cases them elsewhere) to avoid an
      // unintended fall.
      if(interactive()!=nullptr && !interactive()->isLadder())
        setInteraction(nullptr,true);

      if(interactive()==nullptr) {
        const bool noInter = (hnpc->bodystate_interruptable_override!=0);
        if(!noInter) {
          //NOTE: kepp rotation animation: this results in more accurate fight with trolls
          // visual.setAnimRotate(*this,0);
          visual.interrupt(); // TODO: put down in pipeline, at Pose and merge with setAnimAngGet
          }

        if((damageType & (1<<zenkit::DamageType::FLY))==0)
          setAnimAngGet(lastHitType=='A' ? Anim::StumbleA  : Anim::StumbleB);
        }
      }
    }
```

Rationale: `setInteraction(nullptr,true)` is the existing quick/force detach
(same one used on death at npc.cpp:611) and mirrors the original's
`InterruptInteraction` (no `S_..._2_STAND` transition — an immediate yank). The
detach is restricted to interruptable body states (the masked check already
implies `BS_FLAG_INTERRUPTABLE` for `BS_MOBINTERACT_INTERRUPT` /
`BS_ITEMINTERACT`) and skips ladders. The subsequent stumble runs only once the
npc is actually free of the mob, matching the original ordering
(detach -> BS_STUMBLE -> GOTHIT).
