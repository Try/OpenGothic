# Spell damage is not split across its damage-type elements (multi-element spells deal N× too much)

**Confidence:** High (mechanism verified in disassembly; observable only for spells whose
`damage_type` mask has more than one bit set, i.e. multi-element spells).

## Original function + address

In the original game the spell damage descriptor is filled by `ApplyDamages`
(`oDamage.cpp`, Gothic2.exe `0x0065e5a0`), called from
`oCVisualFX::ProcessCollision` (`0x004958d0`). `ProcessCollision` first computes a single
*total* spell damage value `GetDamage() * GetLevel() * collModifier` (where `collModifier`
is 0.5 / 1.0 / 2.0 for the half/normal/double collision flags) and converts it to an
integer. It then passes that one total, together with the spell's damage-type bitmask, to
`ApplyDamages`.

`ApplyDamages` counts how many damage-type bits are set (it accumulates `+1.0` per set bit
into a float `n`), then for every set type whose descriptor slot is still 0 it writes
`round(total / n)` into that slot. The division and rounding are unambiguous in the
disassembly: at `0x0065e76c` the routine executes `fild [total]; fdiv st(1)` (st(1) = the
bit-count `n`), then `fadd 0.5; __ftol` (truncate-toward-zero). So the *sum* of the
per-element damages equals the spell total — a 2-element spell puts `total/2` into each of
its two elements, not `total` into each.

## OpenGothic file:line

- `game/world/objects/npc.cpp:3253-3260` — `Npc::commitSpell`, projectile-spell path.
- `game/world/objects/npc.cpp:2118-2124` — `Npc::takeDamage`, direct-collision spell path.

Both loops assign the full per-element value to *every* set damage-type bit with no division
by the number of elements:

```cpp
// commitSpell (projectile)
for(size_t i=0; i<zenkit::DamageType::NUM; ++i)
  if((spl.damage_type&(1<<i))!=0)
    dmg[i] = spl.damage_per_level*lvl;          // each element gets the FULL total
```

## Divergence

For a spell whose `damage_type` selects a single element (the common case), OpenGothic and
the original agree. For a spell selecting K elements, the original spreads the total over the
elements (`total/K` each, summing back to the total), while OpenGothic writes the full total
into *each* element. After the per-element protection subtraction in
`DamageCalculator::rangeDamage`, OpenGothic therefore deals up to K× the intended damage for
a multi-element spell. (The same over-count exists on the direct-collision path, which also
ignores the spell level — that missing `*lvl` is a separate, already-known item and is left
untouched here.)

## Proposed patch

Divide the assembled per-element value by the number of selected damage types, with the same
`round(x + 0.5)` truncation the original uses. For a single-element spell `K==1`, this is a
no-op, so the change is behaviorally inert except for genuine multi-element spells.

`game/world/objects/npc.cpp` — `Npc::commitSpell` (projectile path):

OLD:
```cpp
  if(active->isSpellShoot()) {
    const int lvl = (castLevel-CS_Emit_0)+1;
    DamageCalculator::Damage dmg={};
    for(size_t i=0; i<zenkit::DamageType::NUM; ++i)
      if((spl.damage_type&(1<<i))!=0) {
        dmg[i] = spl.damage_per_level*lvl;
        }
```
NEW:
```cpp
  if(active->isSpellShoot()) {
    const int lvl = (castLevel-CS_Emit_0)+1;
    DamageCalculator::Damage dmg={};
    // NOTE: in original-game ApplyDamages (Gothic2.exe 0x0065e5a0, called from
    // oCVisualFX::ProcessCollision 0x004958d0) the spell's total damage is split equally
    // across its selected damage-type bits: each element receives round(total/numTypes)
    // (fild;fdiv;fadd 0.5;__ftol @0x0065e76c). Assigning the full total to every element
    // dealt N-times damage for multi-element spells.
    int32_t splTypes = 0;
    for(size_t i=0; i<zenkit::DamageType::NUM; ++i)
      if((spl.damage_type&(1<<i))!=0)
        ++splTypes;
    const int32_t perType = (splTypes>0)
      ? int32_t(float(spl.damage_per_level*lvl)/float(splTypes) + 0.5f) : 0;
    for(size_t i=0; i<zenkit::DamageType::NUM; ++i)
      if((spl.damage_type&(1<<i))!=0) {
        dmg[i] = perType;
        }
```

`game/world/objects/npc.cpp` — `Npc::takeDamage` (direct-collision path):

OLD:
```cpp
  if(isSpell) {
    auto& spl  = owner.script().spellDesc(splId);
    splCat     = SpellCategory(spl.spell_type);
    damageType = spl.damage_type;
    for(size_t i=0; i<zenkit::DamageType::NUM; ++i)
      if((damageType&(1<<i))!=0)
        dmg[i] = spl.damage_per_level;
    }
```
NEW:
```cpp
  if(isSpell) {
    auto& spl  = owner.script().spellDesc(splId);
    splCat     = SpellCategory(spl.spell_type);
    damageType = spl.damage_type;
    // NOTE: in original-game ApplyDamages (Gothic2.exe 0x0065e5a0) the spell total is split
    // equally across its damage-type bits (round(total/numTypes)); see commitSpell.
    int32_t splTypes = 0;
    for(size_t i=0; i<zenkit::DamageType::NUM; ++i)
      if((damageType&(1<<i))!=0)
        ++splTypes;
    const int32_t perType = (splTypes>0)
      ? int32_t(float(spl.damage_per_level)/float(splTypes) + 0.5f) : 0;
    for(size_t i=0; i<zenkit::DamageType::NUM; ++i)
      if((damageType&(1<<i))!=0)
        dmg[i] = perType;
    }
```

Notes:
- `spl.damage_type`, `spl.damage_per_level`, `spl.spell_type`, `zenkit::DamageType::NUM`,
  `DamageCalculator::Damage`, `castLevel`, `CS_Emit_0` are all grep-verified present at
  these lines; no new includes are required (`int32_t`/`float` already used in this file).
- The original folds `collModifier` (0.5/1/2) into the total *before* the split, whereas
  OpenGothic applies the half/double-damage collision flags afterwards in
  `DamageCalculator::rangeDamage`; this leaves a sub-unit rounding difference on
  halved/doubled multi-element hits only and is intentionally left out of this surgical fix.
