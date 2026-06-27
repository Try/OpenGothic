# DamageCalculator: melee attribute bonus is not split across damage types

**Confidence:** High

## Original function + address

`oCNpc::OnDamage_Hit` (Gothic2.exe `0x00666610`), in the "Attribute Boni" block that
runs for a non-spell melee hit from an NPC/player wielding a weapon.

For a melee hit the function builds three booleans from the damage-type mask
(`oSDamageDescriptor` field at `+0x24`): blunt = bit 1 (`& 2`), edge = bit 2 (`& 4`),
point = bit 6 (`& 0x40`). It then counts how many of those three categories are active
into a float (call it `n`):

- `n += 1` for blunt, for edge, and for point that are present.

It fetches `GetAttribute(4)` (STRENGTH) and `GetAttribute(5)` (DEXTERITY), and when
`n != 0` computes the **per-category bonus as `attribute / n`** (a float, truncated to
int via `__ftol`). That same divided bonus is then added to each active accumulator:
STRENGTH/`n` to the blunt accumulator (`+0x30`) and edge accumulator (`+0x34`), and
DEXTERITY/`n` to the point accumulator (`+0x44`). In other words, the strength/dexterity
contribution to a melee hit is **shared equally among all active damage-type categories**,
not added in full to each. The protection subtraction and the `max(...,0)` clamp happen
afterward on `(weaponBaseDamage + attribute/n)`.

This is the documented Gothic 2 behavior (see the "Damage System" forum thread already
cited at the top of `damagecalculator.cpp`): a weapon that deals two damage types splits
the strength bonus between them.

## OpenGothic file:line

`/Users/admin/Downloads/opengothic/game/game/damagecalculator.cpp:178-196`
(`DamageCalculator::swordDamage`, the `version().game==2` branch).

## Divergence

OpenGothic's G2 melee loop adds the **full** attribute to **every** active damage type:

```cpp
const int atr = (i==zenkit::DamageType::POINT) ? dex : str;
int vd = std::max(atr + src.damage[i] - other.protection[i],0);
```

For a single-damage-type weapon (the common case: EDGE-only swords) the count is 1, so
there is no difference. But for a weapon (or monster) whose `damage_type` flags **two or
more** of {BLUNT, EDGE, POINT}, OpenGothic grants the full STR (or DEX) to each type,
whereas the original grants only `attribute / count` to each. A blunt+edge weapon in
OpenGothic therefore receives `STR` extra total damage (`STR/2` over-credited per type),
and monsters with multi-type attacks are similarly inflated.

## Proposed patch

Divide the per-type attribute bonus by the number of active boni-eligible damage
categories (BLUNT/EDGE/POINT), matching the original's `attribute / n` with the same
truncation as `__ftol` (integer division truncates toward zero for the non-negative
operands here).

OLD (`damagecalculator.cpp`, G2 branch of `swordDamage`):
```cpp
    bool invincible = true;
    for(unsigned int i=0; i<zenkit::DamageType::NUM; ++i) {
      if((dtype & (1<<i))==0)
        continue;
      // NOTE: in original-game oCNpc::OnDamage_Hit (Gothic2.exe 0x00666610) the per-type melee
      // attribute boni adds STRENGTH to BLUNT/EDGE but DEXTERITY to POINT (GetAttribute(5)=DEX
      // feeds the point accumulator @desc+0x44, GetAttribute(4)=STR feeds blunt/edge @+0x30/+0x34).
      const int atr = (i==zenkit::DamageType::POINT) ? dex : str;
      int vd = std::max(atr + src.damage[i] - other.protection[i],0);
```

NEW:
```cpp
    // NOTE: in original-game oCNpc::OnDamage_Hit (Gothic2.exe 0x00666610) the melee attribute
    // boni is SPLIT across the active boni categories: it counts how many of BLUNT(bit1)/
    // EDGE(bit2)/POINT(bit6) are set and adds attribute/count (truncated) to each. A weapon
    // that deals two damage types gives only STR/2 (resp. DEX/2) per type. OpenGothic added the
    // full attribute to every type, inflating multi-type weapon and monster hits.
    int boniDiv = 0;
    for(unsigned int i=0; i<zenkit::DamageType::NUM; ++i) {
      if((dtype & (1<<i))==0)
        continue;
      if(i==zenkit::DamageType::BLUNT || i==zenkit::DamageType::EDGE || i==zenkit::DamageType::POINT)
        ++boniDiv;
      }
    boniDiv = std::max(boniDiv,1);

    bool invincible = true;
    for(unsigned int i=0; i<zenkit::DamageType::NUM; ++i) {
      if((dtype & (1<<i))==0)
        continue;
      // NOTE: in original-game oCNpc::OnDamage_Hit (Gothic2.exe 0x00666610) the per-type melee
      // attribute boni adds STRENGTH to BLUNT/EDGE but DEXTERITY to POINT (GetAttribute(5)=DEX
      // feeds the point accumulator @desc+0x44, GetAttribute(4)=STR feeds blunt/edge @+0x30/+0x34),
      // each divided by the number of active boni categories (see boniDiv above).
      const int atr = ((i==zenkit::DamageType::POINT) ? dex : str) / boniDiv;
      int vd = std::max(atr + src.damage[i] - other.protection[i],0);
```

Symbols verified to exist: `zenkit::DamageType::{BLUNT,EDGE,POINT,NUM}`
(`lib/ZenKit/include/zenkit/addon/daedalus.hh:62-70`), `dtype` / `str` / `dex` /
`src.damage[i]` / `other.protection[i]` (existing locals/fields used in the same loop).
Scoped to the G2 branch only; the G1 branch uses the separate `mul`-based formula and was
not analyzed against Gothic1.exe. (Caveat, out of scope: the original adds boni only to
blunt/edge/point accumulators; OpenGothic also adds `atr` to any other active non-boni
type such as FIRE — a distinct pre-existing issue left untouched here.)
