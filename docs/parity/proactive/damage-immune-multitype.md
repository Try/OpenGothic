# Immune (negative-protection) damage types: dropped vs added on multi-type hits

**Confidence:** Medium

## Original behavior

`oCNpc::OnDamage_Hit` (Gothic2.exe @ 0x00666610), the per-damage-type protection
loop (8 iterations over the descriptor damage array). For each active type bit `i`:

```
prot_i = GetProtectionByIndex(i)             // may be negative (immune marker)
if (prot_i < 1) {
    if (stillAllNonPositive && prot_i < 0) invincible = true;
} else {
    stillAllNonPositive = false;
}
total += max(dmg_i - prot_i, 0)              // ALWAYS, for every active type
```

- The accumulation runs for **every** active type, including immune ones: a
  negative protection (e.g. -1) adds `dmg_i + 1`.
- `invincible` is set only when a type has `prot_i < 0` AND no earlier (lower bit
  index) type had `prot_i >= 1`.
- HP is applied at the end only if `invincible == false`; when invincible the hit
  deals 0 HP regardless of the total.

## OpenGothic behavior

`game/game/damagecalculator.cpp` — `rangeDamage(...,Damage,...)` lines 114-122 and
the G2 branch of `swordDamage` lines 156-166:

```cpp
int vd = std::max(... - other.protection[i], 0);
if(other.protection[i]>=0) {   // Filter immune
  value     += vd;
  invincible = false;
  }
```

A type with `protection[i] < 0` is **excluded** from `value`; `invincible` is
false iff at least one active type has `protection >= 0`.

## The divergence

- Single active type (immune, or not): both engines match.
- Multi-type mask mixing an immune type with a non-immune type: the original
  applies HP and the immune type still **adds** `dmg+|prot|`; OpenGothic applies HP
  but the immune type contributes **nothing**.

Example — `damage_type = EDGE | FLY`, target EDGE prot = 50, FLY prot = -1,
`dmgFLY = 10`: original adds `10-(-1)=11` from FLY; OpenGothic drops it.

## Caveats

Only manifests for multi-bit masks mixing immune + non-immune types (uncommon but
reachable, e.g. EDGE+FLY). The original's `invincible` choice is bit-order
dependent, so an exact reproduction is awkward; the patch below matches the
single-type case and approximates the mixed case.

## ⚠ Correction: the approximate patch below is WRONG for some masks — DEFER

Worked through the original's bit-order logic precisely: `invincible` becomes true the
moment an immune type (`prot<0`) is seen *while no armored type (`prot>=1`) has been
seen yet* (bit order: BARRIER,BLUNT,EDGE,FIRE,FLY,MAGIC,POINT,FALL), and once set it is
never cleared. So:
- `EDGE(50)|FLY(-1)` — EDGE (lower bit) clears `stillAllNonPositive` first ⇒ not
  invincible. The patch agrees. ✓
- `FLY(-1)|POINT(50)` — FLY (lower bit) sets invincible *before* POINT is seen ⇒
  **invincible, 0 HP**. The patch below would instead clear invincible (POINT prot>=0)
  and apply damage. ✗ A by-design-immune NPC would become killable.

An exact fix must replicate the `stillAllNonPositive` bit-order state, not just "any
non-immune type ⇒ not invincible". Given this only affects rare mixed immune+armored
multi-bit masks, it is left **deferred** rather than applying the incorrect shortcut.

## Proposed patch (approximate — DO NOT APPLY as-is; see correction above)

File: `game/game/damagecalculator.cpp` — `rangeDamage(Npc&, Npc&, Damage, CollideMask)`

OLD:
```cpp
    int vd = std::max(dmg[size_t(i)] - other.protection[i],0);
    if(other.protection[i]>=0) { // Filter immune
      value     += vd;
      invincible = false;
      }
```

NEW:
```cpp
    int vd = std::max(dmg[size_t(i)] - other.protection[i],0);
    // NOTE: in original-game (oCNpc::OnDamage_Hit @ 0x00666610) every active type
    // accumulates max(dmg-prot,0); a negative protection is only an immune marker
    // that suppresses HP when ALL active types are immune.
    value += vd;
    if(other.protection[i]>=0)
      invincible = false;
```

(Apply the equivalent change to the G2 branch of `swordDamage`, lines 159-165.)
