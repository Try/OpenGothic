# Spell projectile damage zeroed past MaxMagRange (3500cm)

**Confidence:** Medium

## Original behaviour

`oCVisualFX::ProcessCollision` (Gothic2.exe `0x004958d0`) is the function that applies a
flying spell-projectile's damage when the spell VFX physically collides with a vob. After
skipping the caster (its origin vob, descriptor offset `0x4a8`) and de-duplicating already
hit vobs, it walks the collided vob's class chain; if it is an `oCNpc` and
`oCNpc::IsConditionValid` (`0x00735130`) holds, it builds an `oSDamageDescriptor`, scales the
base damage by the collide-mask multiplier (mask bit 0x4 -> 0.5x, bit 0x8 -> 2.0x, else 1.0x),
and posts an `oCMsgDamage`.

Crucially the original applies this damage purely on **physical collision**: there is no
distance / path-length / range comparison anywhere in `ProcessCollision` (verified: no
sqrt/GetDistance/range/length test in the decompilation). How far the magic ball travelled is
irrelevant — the projectile flies until it physically hits something or its VFX trajectory
range / lifetime ends.

## OpenGothic divergence

`game/game/damagecalculator.cpp:73-106` `DamageCalculator::rangeDamage(... const Bullet& b ...)`
is used for both arrows and spells. Line 75 unconditionally sets:

```
bool noHit = dist>float(MaxMagRange);   // MaxMagRange == 3500
```

For arrows the `!b.isSpell()` block (lines 79-103) recomputes `noHit`, so the 3500 value is
overwritten. For **spells** that block is skipped, so `noHit` keeps the `dist>3500` result and
line 105 returns `Val(0,false,invincible)` — i.e. **a spell that hits an NPC after travelling
more than 3500cm deals zero damage**.

Spell projectiles in OG fly until `pathLength()>10000` (`game/physics/dynamicworld.cpp:972`),
so 3500-10000cm hits are reachable (un-homed / long-range combat spells fired across a clearing).
`MaxMagRange` is the `Focus_Ranged` aiming distance (see the constant's own comment), not a
damage cutoff; using it to gate spell damage has no counterpart in the original.

## Proposed patch

```
// game/game/damagecalculator.cpp  (rangeDamage, ~line 74-77)
OLD:
  float dist       = b.pathLength();
  bool  noHit      = dist>float(MaxMagRange);
  bool  invincible = !checkDamageMask(nsrc,nother,&b);
  auto  dmg        = b.damage();

NEW:
  float dist       = b.pathLength();
  // NOTE: in original-game oCVisualFX::ProcessCollision (Gothic2.exe 0x004958d0) a spell
  // projectile applies damage on physical collision with no path-length cutoff; only arrows
  // have a range/hit-chance falloff. MaxMagRange is the Focus_Ranged aiming distance, not a
  // damage gate, so spells must not be zeroed past it.
  bool  noHit      = !b.isSpell() && dist>float(MaxMagRange);
  bool  invincible = !checkDamageMask(nsrc,nother,&b);
  auto  dmg        = b.damage();
```

(The arrow branch immediately below already overwrites `noHit` for `!b.isSpell()`, so the
remaining `&& !b.isSpell()` only changes the spell case: spells now keep `noHit==false` from
distance and proceed to the mask/invincible checks exactly as the original does.)
