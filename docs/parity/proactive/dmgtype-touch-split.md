# TouchDamage multi-type: scalar damage must be SPLIT across set types, not applied in full per type

**Confidence:** High

## Original function + address

`oCNpc::OnDamage_Hit` (Gothic2.exe @ 0x00666610) together with `ApplyDamages`
(Gothic2.exe @ 0x0065e5a0). A `zCTouchDamage` zone (zCTouchDamage::OnTimer @
0x00615c70 / OnTouch @ 0x00615b70) sends an `oCMsgDamage` carrying a *single*
scalar damage value plus a multi-bit damage-type mask; the per-type damage array
in the descriptor is all-zero.

In `OnDamage_Hit`, before the per-type protection loop, the engine sums the
descriptor's per-type damage array over the active type bits. When that sum is
zero (the touch / spell case — only a scalar `damage` was provided), it calls
`ApplyDamages(mask, perTypeArray, &scalarTotal)`. `ApplyDamages` counts the number
of set bits `n` in the mask, computes `scalarTotal / n` (float divide, truncated
to int once), and writes that quotient into every set type slot whose current
value is 0. In other words the **single scalar damage is split evenly across all
set damage types**.

The protection loop then subtracts each type's protection from its (split) share,
floors each share at 0, and sums them into one effective damage that is applied as
a single HP change.

So for a touch zone with damage `D` and `n` set types, the original deals
`sum over set types i of max(D/n - prot_i, 0)` as one HP deduction — never more
than `D` total before protection.

## OpenGothic file:line

`game/world/triggers/touchdamage.cpp` — `TouchDamage::tick` lines ~58-63 and the
helper `TouchDamage::takeDamage` lines ~78-82:

```cpp
for(size_t i=0; i<zenkit::DamageType::NUM; ++i) {
  if(!mask[i])
    continue;
  takeDamage(*npc,int32_t(damage),hnpc.protection[i]);   // full `damage` per type
  }
...
void TouchDamage::takeDamage(Npc& npc, int32_t val, int32_t prot) {
  if(prot<0) // Filter immune
    return;
  npc.changeAttribute(ATR_HITPOINTS,-std::max(val-prot,0),false);
  }
```

OpenGothic applies the **full** `damage` value to **every** set type, as a
**separate** HP deduction per type.

## Divergence

For a single-type touch zone both engines match. For a multi-type touch zone
(`n` set type bits) OpenGothic deals up to `n`x too much damage:

- Original: `sum_i max(D/n - prot_i, 0)`, capped at `D` total pre-protection,
  applied once.
- OpenGothic: `sum_i max(D - prot_i, 0)`, i.e. `D` (minus protection) **per type**,
  applied `n` separate times.

Example — a barrier/fire zone with `damage = 100`, `barrier` + `fire` set, victim
`prot[BARRIER]=0`, `prot[FIRE]=10`:
- Original: split `100/2 = 50` per type → `max(50-0,0) + max(50-10,0) = 50 + 40 =
  90` HP, once.
- OpenGothic: `max(100-0,0)` then `max(100-10,0)` → `100 + 90 = 190` HP across two
  deductions — more than double.

## Proposed patch

File: `game/world/triggers/touchdamage.cpp` — `TouchDamage::tick`

OLD:
```cpp
    for(size_t i=0; i<zenkit::DamageType::NUM; ++i) {
      if(!mask[i])
        continue;
      takeDamage(*npc,int32_t(damage),hnpc.protection[i]);
      }
```

NEW:
```cpp
    // NOTE: in original-game (oCNpc::OnDamage_Hit @0x00666610 -> ApplyDamages
    // @0x0065e5a0) a touch zone provides a single scalar `damage` and a multi-bit
    // type mask; the engine splits damage evenly across the set types
    // (damage / numSetTypes, truncated), subtracts per-type protection, floors
    // each share at 0, sums them, and applies the result as one HP change.
    // Applying the full `damage` per type (as before) over-damaged multi-type
    // zones by up to numSetTypes-fold.
    int32_t nTypes = 0;
    for(size_t i=0; i<zenkit::DamageType::NUM; ++i)
      if(mask[i])
        ++nTypes;
    if(nTypes>0) {
      const int32_t share = int32_t(damage)/nTypes;
      int32_t total = 0;
      for(size_t i=0; i<zenkit::DamageType::NUM; ++i) {
        if(!mask[i] || hnpc.protection[i]<0) // skip unset & immune types
          continue;
        total += std::max(share-hnpc.protection[i],0);
        }
      npc->changeAttribute(ATR_HITPOINTS,-total,false);
      }
```

(The `takeDamage` helper is then unused for this path; it may be left in place or
removed — removing it is not required for the fix.)

## Caveats

- The split rounding matches the original's truncating integer divide
  (`int32_t(damage)/nTypes`).
- The per-type immune filter (`protection[i] < 0` skipped) is kept as-is; the
  original's subtle bit-order "whole-hit immune" interaction for mixed
  immune+armored masks is tracked separately in
  `damage-immune-multitype.md` (DEFERRED) and is out of scope here.
- The deep-water DAM_BARRIER instant-kill branch above this loop
  (`dmgmode-barrier-water.md`) and the repeat-delay gating
  (`dot-touchdamage-repeat-zero.md`) are unaffected.
