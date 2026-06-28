# Damage-type → protection resolution / multi-type damage — fresh-divergence survey

**Confidence:** N/A — **NO FINDING** (no surgical, build-verifiable, *fresh* divergence)

## Original fn + address

`oCNpc::OnDamage_Hit` (Gothic2.exe `0x00666610`). Sub-helpers checked:
`oCNpc::GetProtectionByIndex` (`0x0072fc20` — a bare `return protection[i]`, i.e.
`*(int*)(this + i*4 + 0x1ec)`, no magic-circle/armor adjustment at lookup) and
`oCNpc::GetProtectionByMode` (`0x0072fcd0`). I walked the full descriptor pipeline:
the glancing/critical multiplier build (`desc+0x50`), the per-type multiply+`ftol`
loop, the `iVar4 == 0` "Split"/`ApplyDamages` path (total split across active types,
monster STR fallback when total==0), the attribute-boni section (STR→BLUNT/EDGE,
DEX→POINT, divided by active-category count), and the final 8-iteration protection
loop (`oVar16 = 0..7`, `bit = 1<<i` tested against the mask at `desc+0x24`,
`eff = max(damage[i] - GetProtectionByIndex(i), 0)`, summed into `desc+0x4c`), plus
the trailing immortal zero-out `if (HasFlag(this,2) || immuneFlag) desc+0x4c = 0`.

## OG file:line

`/Users/admin/Downloads/opengothic/game/game/damagecalculator.cpp`
— `rangeDamage(Damage)` 125-146, `swordDamage` 148-233, `damageTypeMask` 235-239,
`checkDamageMask` 241-260, `rangeDamageValue` 262-272.

## Divergence

The damage-type-bit → `protection[]`/`damage[]` index mapping is **faithful**:
both engines use a direct 1:1 index `i` over the 8 types
(BARRIER,BLUNT,EDGE,FIRE,FLY,MAGIC,POINT,FALL — `zenkit::DamageType::NUM == 8`,
ordering confirmed against `constants.h` `PROT_*`), the same `1<<i` mask gate, the
same `max(dmg-prot,0)` per-type subtraction, and the same direct `protection[i]`
lookup (no per-type FIRE/FLY/FALL special-casing; `GetProtectionByIndex` is a plain
array read). Every concrete behavioral delta I could substantiate in this area is
already covered:

- **immune (-1) per-type accumulation** and the **order-dependent whole-hit
  zero-out** (immune type before first `prot>=1` type) → `prot-immune-accumulation.md`
  and `damage-immune-multitype.md` (both verified 1:1 and **deferred**: vanilla
  weapons/spells are single-type, so no vanilla outcome changes).
- **attribute-boni split** by active BLUNT/EDGE/POINT count, STR vs DEX per type →
  `dmg2-attribute-boni-split.md` / `dmgform-point-uses-dexterity.md` (fixed, lines
  177-199).
- **spell multi-element / total split** → `spelldmg-multielement-split.md`,
  `wpnhit-damagetotal-spread.md`, `dmgtype-touch-split.md`.
- **non-spell MinDamage floor** → `damage-minfloor-spells.md` (fixed, lines 33-39).
- **DAM_BARRIER swim instakill** → `dmgmode-barrier-water.md`.

The one **undocumented** original behavior I found is the "Assuming Edge Damage / No
primary damage type" auto-EDGE fallback (`OnDamage_Hit` ~`0x0066b4xx`): inside the
attribute-boni branch, for a non-spell semi-human attacker whose damage-type mask has
**none** of BLUNT/EDGE/POINT set (and a vtable-`+0x10c` attacker predicate holds), the
original forces `desc+0x24 |= DAM_EDGE` — which then makes the protection loop resolve
against `protection[EDGE]`. OpenGothic's `swordDamage` has no such fallback (a mask
with no physical type simply contributes nothing).

This is **not** turned into a fix because it is neither high-confidence nor surgical:
(1) it is gated on an unresolved virtual call (`vtbl+0x10c`) plus the not-a-spell and
semi-human conditions; (2) it triggers only for the practically-nonexistent case of a
human/fist attacker whose `damagetype` mask contains zero of BLUNT/EDGE/POINT — vanilla
weapons are single-type EDGE/BLUNT and fists resolve to a physical type, so vanilla
impact is ~nil; and (3) the patch site is the attribute-boni / `boniDiv` block that was
just carefully reconstructed, so editing it for a non-vanilla edge case risks regressing
the common single-type path. Per "empty beats false positives," this is left as an
observation rather than a patch.

## Proposed patch

**NO FINDING.** The damage-type → protection mapping and multi-type handling are
faithful; all real deltas are already fixed or explicitly deferred. The only fresh
candidate (auto-EDGE fallback) fails the surgical / high-confidence bar and has
negligible vanilla impact — deferred, not applied.
