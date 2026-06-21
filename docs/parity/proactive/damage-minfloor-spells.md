# NPC_MINIMAL_DAMAGE floor wrongly applied to spell damage

**Confidence:** Medium

## Original behavior

`oCNpc::OnDamage_Hit` (Gothic2.exe @ 0x00666610), end of function.

After the protection loop accumulates effective damage and it is clamped to >= 0,
a minimum-damage floor is applied. The engine caches `NPC_MINIMAL_DAMAGE` from the
scripts (retail G2 value = 5). The floor is applied only when **all** hold:
- descriptor magic/spell vob pointer (field `+0xd0`) is null, and
- descriptor associated-object pointer (field `+0x10`) is null, and
- effective damage `< NPC_MINIMAL_DAMAGE`.

```
dmg = max(accumulated, 0)
if (descriptor.magicVob == null && descriptor.assocObj == null && dmg < NPC_MINIMAL_DAMAGE)
    dmg = NPC_MINIMAL_DAMAGE
```

A spell hit's descriptor carries a magic vob (`+0xd0 != 0`), so the floor is
skipped for spells. Only plain physical (melee / ranged-projectile, non-magic)
hits get the 5-point floor; resisted spells can deal < 5 (or 0).

## OpenGothic behavior

`game/game/damagecalculator.cpp:33-34` (`DamageCalculator::damageValue`):

```cpp
if(ret.hasHit && !ret.invincible && Gothic::inst().version().game==2)
  ret.value = std::max<int32_t>(ret.value,MinDamage);   // MinDamage == 5
```

`damageValue` is reached for spells too: `Npc::takeDamage(...,vfx,splId)`
(`npc.cpp:2048`) -> `takeDamage(...,isSpell=true)` (`npc.cpp:2063`) ->
`damageValue(...,isSpell,...)` (`npc.cpp:2091`). The floor ignores `isSpell`.

## The divergence

A spell landing for < 5 effective damage (after magic-type protection) is forced
up to 5 in OpenGothic; the original never floors magic hits. Weak / heavily
resisted spells therefore hit harder in OpenGothic.

## Proposed patch

File: `game/game/damagecalculator.cpp` (`bullet.h` already included).

OLD:
```cpp
  if(ret.hasHit && !ret.invincible && Gothic::inst().version().game==2)
    ret.value = std::max<int32_t>(ret.value,MinDamage);
  return ret;
```

NEW:
```cpp
  // NOTE: in original-game (oCNpc::OnDamage_Hit @ 0x00666610) the NPC_MINIMAL_DAMAGE
  // floor is applied only when the descriptor has no magic vob (field +0xd0),
  // i.e. for plain physical hits. Spell hits are never floored.
  const bool spellHit = isSpell || (b!=nullptr && b->isSpell());
  if(ret.hasHit && !ret.invincible && !spellHit && Gothic::inst().version().game==2)
    ret.value = std::max<int32_t>(ret.value,MinDamage);
  return ret;
```
