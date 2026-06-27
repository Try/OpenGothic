# Parry/block spark (CS_IAI / CPFX_IAI) ignores material-pair canonical ordering

**Confidence:** Medium-High (clear logic divergence; the audible/visible "silent in one
direction" consequence is a strong inference from why the original sorts).

## Original fn + address
`oCAniCtrl_Human::StartParadeEffects` @ `0x006b16f0` (called from `oCNpc::EV_Parade`
@ `0x007522d0`). When a parry connects the engine reads the *defender's* weapon-attack
sound material (its own `GetSoundMaterial_MA`, vtbl `+0xf8`) and the *attacker's* material,
looks each up in the per-material name table at `0xab3474` (stride `0x14`), and then builds a
**single combined material token** for the spark FX name `CPFX_IAI_<token>` (literal prefix at
`0x8b1c48` = `"CPFX_IAI_"`), passed to `DoSparks`; the parry sound is then emitted by
`zCSoundManager::StartAttackSound`. The token is built by **comparing the two material strings
with `zSTRING::compare` and emitting them in ascending (canonical) order**: there are two
symmetric branches keyed on the sign of the compare (`iVar3 < 0` vs `> 0`) that concatenate the
two codes in opposite operand order, so the smaller code is always placed first. The result is
therefore **independent of who is attacker vs. defender** — a metal-blade-vs-wooden-club parry
yields the same `..._ME_WO` name in both matchup directions. (The spark is also gated on neither
combatant being in fist mode, `GetWeaponMode != 1`.)

## OG file:line
`game/world/world.cpp:775-778` — `World::addWeaponBlkEffect`
(reached via `game/graphics/mdlvisual.cpp:406-422` `MdlVisual::emitBlockEffect`,
called from `game/world/objects/npc.cpp:2112-2113` on a successful block).

```cpp
Sound World::addWeaponBlkEffect(ItemMaterial src, ItemMaterial reciver, const Tempest::Matrix4x4& pos) {
  // IAI - item attacks item
  return addHitEffect(materialTag(src),materialTag(reciver),"IAI",pos);
  }
```

`emitBlockEffect(dest=defender, source=attacker)` passes `src = attacker's weapon material`,
`reciver = defender's weapon material`, so `addHitEffect` builds `CS_IAI_<attacker>_<defender>`
and `CPFX_IAI_<attacker>_<defender>` in a **fixed attacker-first order, with no sorting**.

## Divergence
OpenGothic's parry sound/spark name depends on the attacker/defender direction, whereas the
original canonicalizes the material pair (ascending). For a mixed-material parry (e.g. metal blade
vs. wooden club) OpenGothic produces `CS_IAI_ME_WO` in one matchup direction and `CS_IAI_WO_ME` in
the other. Gothic's `Sfx.d`/PFX set defines the IAI pair in only one canonical order (precisely the
reason the engine sorts), so one of OpenGothic's two orderings resolves to an undefined SFX/PFX and
the parry clang + spark go silent depending on who is attacking whom. Same-material parries
(`ME_ME`, `WO_WO`) are unaffected.

## Proposed patch
Canonicalize the two material tags ascending before building the name, mirroring
`StartParadeEffects`'s `zSTRING::compare`-keyed ordering (`materialTag` returns the same 2-char
codes the original compares, so `std::string_view` ordering is equivalent).

OLD (`game/world/world.cpp:775-778`):
```cpp
Sound World::addWeaponBlkEffect(ItemMaterial src, ItemMaterial reciver, const Tempest::Matrix4x4& pos) {
  // IAI - item attacks item
  return addHitEffect(materialTag(src),materialTag(reciver),"IAI",pos);
  }
```

NEW:
```cpp
Sound World::addWeaponBlkEffect(ItemMaterial src, ItemMaterial reciver, const Tempest::Matrix4x4& pos) {
  // IAI - item attacks item
  // NOTE: in original-game oCAniCtrl_Human::StartParadeEffects @0x006b16f0 the two weapon
  // materials are sorted via zSTRING::compare (smaller code first) before forming the
  // CPFX_IAI_/sound token, so the parry FX name is independent of attacker-vs-defender order.
  // OpenGothic passed them attacker-first unsorted, so a mixed-material parry resolved to a
  // different (often undefined) SFX/PFX in one of the two matchup directions -> silent parry.
  std::string_view a = materialTag(src);
  std::string_view b = materialTag(reciver);
  if(b < a)
    std::swap(a,b);
  return addHitEffect(a,b,"IAI",pos);
  }
```

(`<string_view>`/`std::swap` are already transitively available via `string_frm`/STL in this TU;
add `#include <algorithm>` if the build complains.)
