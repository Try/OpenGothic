# Flesh/blood weapon-hit FX fires on fully-absorbed (zero real-damage) melee hits

**Confidence:** Medium-High

## Original function + address

`oCNpc::OnDamage_Effects_Start` (Gothic2.exe @ `0x006ee40` / warm-decompiler entry `0x00670350`)
is the routine that emits the blood-splat decal and the flesh blood-particle on a melee hit.
Near its start it captures the damage descriptor's *effective/real damage* field
(descriptor offset `0xa4`, labelled "Effective Damage" / "Real Damage" in the debug-spy strings)
into a local and computes `bVar6 = (realDamage > 0)`. The blood/flesh branch
("Hit causes blood") is entered only when, in addition to other gates, that real-damage value
is strictly positive:

```
bloodPath = (HasFlag(this,2)==0)        // receiver is not flag-2 (no-blood / immortal class)
          && (damageFlagsBit8 == 0)     // not the FLY/knock damage-type bit
          && (realDamage > 0)           // <-- effective damage after protection must be > 0
          && (not the special-inflictor case)
```

The descriptor's `0xa4` field is set in `oCNpc::OnDamage_Hit` (@ `0x00666610`) from the
post-protection damage and is **clamped to 0** when the computed real damage is negative
(`if (realDamage < 0) realDamage = 0;`). Thus, when the receiver's protection fully absorbs
the strike (or the receiver is immune to the weapon's damage type), real damage is 0 and the
original emits **no** blood-splat / flesh hit-particle. (The unconditional swing/attack *sound*
is emitted separately in `oCAniCtrl_Human::CreateHit` @ `0x006b0830` via
`zCSoundManager::StartAttackSound`, so this gate only suppresses the visual flesh FX, not the
weapon swing audio.)

## OpenGothic file:line

`game/world/objects/npc.cpp:2121-2122`

```cpp
hitResult = DamageCalculator::damageValue(other,*this,b,isSpell,dmg,bMask);
if(!isSpell && !isDown() && hitResult.hasHit)
  owner.addWeaponHitEffect(other,b,*this).play();
```

`World::addWeaponHitEffect` (`game/world/world.cpp:730`) builds the flesh/material collision FX
`CS_IAM_<weaponMat>_<armorTag>` + `CPFX_IAM_<weaponMat>_<armorTag>`, with `armorTag` defaulting
to `"FL"` (flesh) unless the receiver wears metal/wood armor — i.e. this is exactly the
flesh-impact (blood-spark) visual that the original gates.

## Divergence

OpenGothic plays the flesh/material hit FX whenever `hitResult.hasHit` is true, **without**
checking that any real damage was actually dealt. `DamageCalculator::swordDamage` /
`rangeDamage` return `Val(value, hasHit=true, invincible)` where `value` can be `0` while
`hasHit==true`:

- against a target **immune** to the weapon's damage type, every per-type contribution is
  filtered out, `invincible` stays true, `value==0`, and the G2 `MinDamage` floor at
  `damagecalculator.cpp:37-38` is *not* applied (guarded by `!ret.invincible`);
- so `hasHit==true && value==0`, and OG still fires the `"FL"` (flesh) hit FX.

The original suppresses the blood/flesh FX in this case because its `realDamage(0xa4) > 0`
gate is false. Result: in OpenGothic an immune / fully-absorbed melee hit produces a spurious
flesh/blood impact spark that the original never shows.

## Proposed patch

Gate the flesh/material hit-effect on real damage being dealt, mirroring the original's
`realDamage > 0` condition. `hitResult.value` is the post-protection damage and is `0` for an
immune/fully-absorbed hit, so it is the direct analogue of descriptor field `0xa4`.

OLD (`game/world/objects/npc.cpp:2121-2122`):
```cpp
  if(!isSpell && !isDown() && hitResult.hasHit)
    owner.addWeaponHitEffect(other,b,*this).play();
```

NEW:
```cpp
  // NOTE: in original-game oCNpc::OnDamage_Effects_Start (Gothic2.exe @0x006ee40) the blood/flesh
  // hit-particle is gated on the descriptor's real-damage field (offset 0xa4, set & clamped >=0 in
  // oCNpc::OnDamage_Hit @0x00666610) being > 0; a fully-absorbed/immune hit (value==0) shows no
  // flesh FX, only the separately-emitted swing sound. Mirror that gate here.
  if(!isSpell && !isDown() && hitResult.hasHit && hitResult.value>0)
    owner.addWeaponHitEffect(other,b,*this).play();
```

### Caveat / residual concern (why Medium-High, not High)

`World::addWeaponHitEffect` returns a `Sound` and the original splits the always-on attack
*sound* (`StartAttackSound`, ungated) from the gated blood/flesh *particle*. OpenGothic
conflates both into the one `addWeaponHitEffect` call, so the proposed gate also suppresses
the collision *sound* on a zero-damage hit. In the original the weapon-swing sound comes from
`CreateHit`/`StartAttackSound` (a different code path) and would still play, so the audio
behaviour is not perfectly 1:1 after this change. If exact sound parity matters, the
sound and the `CPFX` particle should be separated and only the particle gated; that is a
larger refactor. The damage>0 gate on the *visual* flesh FX, however, matches the original
and removes the clearly-wrong spurious blood spark on immune hits.
