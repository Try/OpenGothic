# Player-with-drawn-weapon is hard-interrupted/stumbled on hit (missing queue-vs-interrupt guard)

**Confidence:** High (root cause identified in the original damage-animation gate; OG omits the player/weapon branch entirely). Medium on the exact patch shape, because the original also queues a `T_GOTHIT` reaction that OG never models — the safe, surgical fix is only to restore the *suppression* of the hard interrupt for an armed player.

## Original function + address (prose only)

`oCNpc::OnDamage_Anim` (Gothic2.exe @0x00675bd0) makes the engine-level "got-hit reaction" decision. Just before choosing between two branches it computes two player-only predicates:

- a flag (call it *armedPlayer*) that is true only when the victim is the player **and** `oCNpc::GetWeaponMode` (@0x00738c40) returns a value in the inclusive range 1..7 — i.e. the player currently has fists or any weapon drawn (fist/1H/2H/bow/crossbow/magic).
- a second player-only flag tied to an internal player field (`this+0x438`).

The branch selector is: `if (armedPlayer || oCNpc::IsBodyStateInterruptable(this)==0 || playerFlag438)`.

- **Queue branch (taken when the selector is true):** the engine constructs an `oCMsgConversation` carrying the animation name `"T_GOTHIT"` and **enqueues** it on the NPC's event manager (non-interrupting; it plays behind whatever the NPC is currently doing). No interrupt, no forced stumble.
- **Interrupt branch (the `else`):** `ClearEM` (flush the queue) → `oCNpc::Interrupt(this,0,0)` (@0x00735ab0, which forces `oCAniCtrl_Human::HitInterrupt` @0x006b11a0 and tears down interruptable layers) → `SetBodyState(BS_STUMBLE)` → immediately play the interrupt-prefixed stumble animation.

So in the original, a player who is hit **while a weapon/fists are drawn** is *never* hard-interrupted into a stumble; it only ever receives the queued, non-interrupting `T_GOTHIT` reaction. The hard interrupt+stumble is reserved for the non-armed-player case (and is itself still gated by `IsBodyStateInterruptable`, which additionally returns 0 whenever any BS_MOD_* modifier bit — hidden/drunk/nuts/burning/controlled/transformed — is set; original mask `0x3f80` over the bodystate word at `this+0x76c`).

`oCNpc::IsBodyStateInterruptable` (@0x0075efa0) ≡ `(bodystate & BS_MOD_MASK)==0 && (bodystate & BS_FLAG_INTERRUPTABLE)!=0`.

## OpenGothic file:line

`game/world/objects/npc.cpp:2129-2143` (inside `Npc::takeDamage(...)`).

```cpp
if(hitResult.hasHit) {
  auto state = bodyStateMasked();
  if(interactive()==nullptr && ((state&BS_FLAG_INTERRUPTABLE)!=BS_NONE || state==BS_RUN || state==BS_NONE)) {
    //NONE/RUN requires for monsters like waran
    const bool noInter = (hnpc->bodystate_interruptable_override!=0);
    if(!noInter) {
      //NOTE: kepp rotation animation: ...
      visual.interrupt(); // TODO: put down in pipeline, ...
      }
    if((damageType & (1<<zenkit::DamageType::FLY))==0)
      setAnimAngGet(lastHitType=='A' ? Anim::StumbleA  : Anim::StumbleB);
    }
  }
```

## Divergence

OpenGothic has **no armed-player guard**. For the player, as long as the (masked) bodystate is interruptable (any combat stance: `BS_HIT`, attack/parade frames that carry the interruptable flag, `BS_RUN`, etc.), OG runs `visual.interrupt()` (hard interrupt of every interruptable layer) and force-plays the stumble. The original suppresses exactly this path for an armed player and substitutes a queued, non-interrupting `T_GOTHIT`. Observable effect: in OpenGothic the player visibly stumbles / loses their combat animation when struck mid-combat with a weapon drawn, where stock Gothic II keeps the player on their feet (the player keeps swinging/parrying; only a queued got-hit reaction shows). This is the core queue-vs-interrupt decision of the bodystate-interrupt subsystem.

Secondary (lower-priority, latent) note: OG's interruptability test ignores BS_MOD_* modifier bits, both because `bodyStateMasked()` strips them (`bs & (BS_MAX | BS_FLAG_MASK)`, dropping `BS_MOD_MASK`) and because OG never sets those runtime modifier bits at all. The original would treat a transformed/burning/controlled NPC as *not* interruptable. This is currently unobservable in OG (the modifiers aren't modelled) and is therefore **DEFERRED** — it cannot be exercised until BS_MOD_* are actually maintained.

## Proposed patch

Restore the original's "do not hard-interrupt an armed player" suppression. The minimal, build-verifiable change keeps OG's existing stumble for the non-armed case but, for an armed player, skips both the hard interrupt and the forced stumble (OG does not model the queued `T_GOTHIT` reaction, so the closest faithful behavior is "no engine-forced reaction", matching the original's non-interrupting outcome).

Grep-verified OG symbols used: `isPlayer()` (npc.h:116), `weaponState()` returning `WeaponState` with `WeaponState::NoWeapon` (npc.h:264, constants.h:201-208), `bodyStateMasked()` (npc.h:247), `BS_FLAG_INTERRUPTABLE`/`BS_RUN`/`BS_NONE` (constants.h), `visual.interrupt()` (pose.cpp:255), `setAnimAngGet` (npc.cpp:988).

OLD (`game/world/objects/npc.cpp`, ~2129-2143):
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
    // NOTE: in original-game oCNpc::OnDamage_Anim @0x00675bd0 the player is NOT hard-interrupted
    // into a stumble while a weapon/fists are drawn (GetWeaponMode @0x00738c40 in 1..7); the engine
    // queues a non-interrupting T_GOTHIT reaction instead. The forced interrupt+stumble is reserved
    // for the non-armed case (and still gated by IsBodyStateInterruptable @0x0075efa0).
    const bool armedPlayer = isPlayer() && weaponState()!=WeaponState::NoWeapon;
    if(interactive()==nullptr && !armedPlayer &&
       ((state&BS_FLAG_INTERRUPTABLE)!=BS_NONE || state==BS_RUN || state==BS_NONE)) {
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

Caveat for the reviewer: the original's queue branch still shows a queued `T_GOTHIT` got-hit reaction for the armed player; OG has no equivalent EM-queued conversation message here, so this patch reproduces the original's *non-interrupting* outcome but not the queued reaction animation. If exact parity of the `T_GOTHIT` reaction is desired, that requires wiring a non-interrupting queued animation, which is a larger change and should be a separate item.
