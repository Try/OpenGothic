# Hit reaction: armed NPCs stumble where original sends only T_GOTHIT

> DEFER: hinges on whether monsters (trolls/warans, which the surrounding OG comments explicitly tuned the interrupt/stumble for) report weaponState==NoWeapon or Fist. If a monster is 'armed' (Fist mode), the weaponDrawn guard would wrongly suppress its stumble/interrupt and regress monster combat. Needs the monster weaponState confirmed + runtime before applying.

**Confidence:** Medium

## Original function

`oCNpc::OnDamage_Anim` (Gothic2.exe `0x00675bd0`). For a hit that "causes
stumble" (the branch reached after the interrupt-eligibility gate), the engine
splits into two sub-cases:

- It computes a flag that is true when the NPC is alive **and** its weapon mode
  is a drawn-weapon mode (weaponMode in 1..7: fist/1h/2h/bow/xbow/mag), and a
  second flag for the alive+magic-transform case.
- If (weapon-drawn) **or** `IsBodyStateInterruptable()` is false **or**
  (magic-transform): it creates and queues a non-interrupting `T_GOTHIT`
  `oCMsgConversation` (type 1) reaction — the NPC registers the hit but keeps
  its stance; it does **not** play a stumble animation and does **not** change
  body state.
- Only in the remaining case (idle / weapon stowed / body state interruptable)
  does it `ClearEM` + `Interrupt` + `SetBodyState(BS_STUMBLE)` and play the
  `T_STUMBLE`/`T_STUMBLEB` animation.

So: an NPC with a weapon drawn that is hit while standing does **not** stumble
in the original — it takes the soft `T_GOTHIT` path.

## OpenGothic

`game/world/objects/npc.cpp:2105-2119` (`Npc::takeDamage`). The stumble block is
guarded only by `interactive()==nullptr`, the body-state interruptable test, and
`bodystate_interruptable_override`. There is **no** weapon-drawn / weapon-mode
guard. `BS_STAND` (a fighting NPC's idle combat stance) carries
`BS_FLAG_INTERRUPTABLE`, so an armed enemy standing between attacks is hit ->
`visual.interrupt()` + `setAnimAngGet(Anim::StumbleA/B)` => full `T_STUMBLE`.

## Divergence

Armed enemies stumble on every interruptable-state hit in OG, whereas the
original suppresses the stumble for drawn-weapon NPCs and instead queues a
`T_GOTHIT` reaction. Gameplay effect: armed foes are far easier to stagger /
stun-lock in OG than in the retail game.

## Proposed patch

```cpp
// game/world/objects/npc.cpp  (~2116)
```
OLD:
```cpp
      if((damageType & (1<<zenkit::DamageType::FLY))==0)
        setAnimAngGet(lastHitType=='A' ? Anim::StumbleA  : Anim::StumbleB);
```
NEW:
```cpp
      // NOTE: in original-game oCNpc::OnDamage_Anim (Gothic2.exe 0x00675bd0)
      // a stumble animation is played only when the weapon is stowed; with a
      // weapon drawn (weaponMode 1..7) the engine queues a soft T_GOTHIT
      // reaction instead and does not stumble. Without this guard armed enemies
      // get stun-locked far more easily than in the retail game.
      const bool weaponDrawn = (weaponState()!=WeaponState::NoWeapon);
      if((damageType & (1<<zenkit::DamageType::FLY))==0 && !weaponDrawn)
        setAnimAngGet(lastHitType=='A' ? Anim::StumbleA  : Anim::StumbleB);
```

Note: this only suppresses the visual stumble for armed NPCs. OG has no
`T_GOTHIT` EM-message pipeline, so the soft reaction is simply omitted (matching
the observable "no stumble" outcome). The `visual.interrupt()` above is left
untouched; the original's armed path also avoids a hard interrupt, so a stricter
fix would gate `visual.interrupt()` on `!weaponDrawn` as well, but that risks
regressing the troll/waran cases noted in the surrounding comments, so it is left
out of this conservative patch.
