# Talent/hitchance: unarmed (fist) melee wrongly rolls the glancing-blow reduction

**Confidence:** High (structural divergence proven from decompiled original; gameplay impact on player fist combat)

## Original function + address (prose)

`oCNpc::OnDamage_Hit` @ `0x00666610` is the melee damage application in `Gothic2.exe`.
After accumulating the per-damage-type base value (attribute bonus + weapon damage -
target protection), the engine performs the "glancing blow" probability roll. That roll
exists in exactly two places, and both are gated on the attacker's drawn weapon mode:

- when `GetWeaponMode() == 3` (one-handed melee drawn) it reads
  `GetHitChance(1)` and rolls `if (hitchance < rand()%100 + 1)` → glancing;
- when `GetWeaponMode() == 4` (two-handed melee drawn) it reads
  `GetHitChance(2)` and rolls the same way.

`oCNpc::GetHitChance(idx)` returns `*(this + idx*4 + 0x1d8)`, i.e. the hitchance array is
indexed by TALENT id (1=1H, 2=2H, 3=BOW, 4=CROSSBOW); slot 0 (`0x1d8`, TALENT_UNKNOWN) is
never consulted in this path. Crucially, an **unarmed/fist attack** uses the FIST weapon
mode (not 3 and not 4), so it enters **neither** branch: fist hits receive **no** glancing
reduction and deal full attribute-based damage. Monsters likewise attack in FIST mode and
therefore also skip the reduction (the well-known "monsters always crit" effect).

## OpenGothic file:line

`game/game/damagecalculator.cpp:158`, `:165-169`, `:186-187` (`DamageCalculator::swordDamage`, the G2 branch).

## Divergence

OpenGothic determines the talent from the active weapon:

```
Talent tal = TALENT_UNKNOWN;            // line 158
...
if(auto w = nsrc.inventory().activeWeapon()) {   // line 165
  if(w->is2H()) tal = TALENT_2H; else tal = TALENT_1H;
}
...
if(src.hitchance[tal]<=critChance)      // line 186
  vd = (vd-1)/10;
```

For an **unarmed** attacker `activeWeapon()` is null, so `tal` stays `TALENT_UNKNOWN` (0)
and the code rolls the glancing reduction using `src.hitchance[0]`. The original engine
never performs this roll for fist attacks. Because the unused hitchance slot 0 is
effectively 0 on a normal C_NPC, `hitchance[0] <= critChance` (critChance = `rand(100)` =
0..99) is essentially always true, so OpenGothic reduces every player fist hit to
`(vd-1)/10` (then clamped up to `MinDamage`=5 at line 37-38), whereas the original deals
the full `STR + fist_damage - protection`. Player fist/brawl damage is therefore far lower
than in the original.

Monsters are unaffected: OpenGothic already forces `critChance = -1` for
`isMonster() && tal==TALENT_UNKNOWN` (line 172-175), so `hitchance[0] <= -1` is false and
they keep full damage — matching the original's FIST-mode behaviour. Only the (non-monster)
unarmed-player case diverges.

## Proposed patch

Guard the glancing-blow roll so it runs only when a melee weapon (1H/2H) is equipped,
exactly mirroring the original's `WeaponMode==3 || ==4` gating. Unarmed attacks then keep
full damage.

OLD (`game/game/damagecalculator.cpp:186-187`):
```cpp
      if(src.hitchance[tal]<=critChance)
        vd = (vd-1)/10;
```

NEW:
```cpp
      // NOTE: in original-game oCNpc::OnDamage_Hit @0x00666610 the glancing-blow roll
      // (hitchance < rand%100+1) runs only for WeaponMode 3 (1H, GetHitChance(1)) and 4
      // (2H, GetHitChance(2)); unarmed/fist attacks use the FIST mode and skip it, dealing
      // full damage. tal stays TALENT_UNKNOWN for fists, so guard the roll on a melee weapon.
      if(tal!=TALENT_UNKNOWN && src.hitchance[tal]<=critChance)
        vd = (vd-1)/10;
```

Grep-verified OG symbols: `TALENT_UNKNOWN` (`game/game/constants.h:446`), local `tal` of
type `Talent` (`damagecalculator.cpp:158`), `src.hitchance[]` (zenkit handle, used at
`damagecalculator.cpp:186`), `critChance` (`damagecalculator.cpp:161`). The existing
`critChance=-1` monster shortcut (line 172-175) becomes redundant but stays harmless and is
left untouched to keep the patch minimal.
