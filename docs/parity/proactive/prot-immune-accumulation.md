# Protection immune (-1) handling: per-type accumulation & order-dependent zero-out

**Confidence:** Medium. The logic divergence is verified 1:1 against the binary and is
certain; observable behavior only differs for **multi-bit damage-type attacks** (a victim
immune to one active type but not another). Such attacks do not occur in vanilla Gothic 2
(weapons are single-type EDGE/BLUNT/POINT, spells single-type FIRE/MAGIC), so vanilla impact
is negligible. Mods that define a multi-type `damagetype` mask would be affected.

## Original function + address
`oCNpc::OnDamage_Hit` (Gothic2.exe `0x00666610`). The protection-subtraction loop sits after
the attribute-boni / multiplier / glancing build phase. Walking it in prose:

- A running total starts at 0. Two booleans seed the loop: an "immune-candidate" flag seeded
  *true*, and an "immune" flag seeded *false*.
- For each damage-type index `i` in **0..7 (ascending index order)** whose bit is set in the
  descriptor's damage-type mask:
  - reads the per-type built-up raw damage and `prot = GetProtectionByIndex(i)`
    (`0x0072fc20`, which simply returns the stored `protection[i]` — no magic-circle or armor
    bonus is added at lookup time).
  - if `prot < 1` (i.e. `prot <= 0`): when the immune-candidate flag is *still true* **and**
    `prot < 0`, it sets the immune flag *true*. A `prot == 0` type does **not** touch the
    immune-candidate flag.
  - else (`prot >= 1`): it clears the immune-candidate flag.
  - **regardless of sign**, it then computes `eff = max(raw - prot, 0)` and adds `eff` to the
    running total (note: for an immune type with `prot == -1`, `eff = raw + 1`, a positive
    contribution).
- After the loop the total is stored, and later (with the immortal `HasFlag(...,2)` check) the
  **whole** total is forced to 0 if the immune flag is set; HP is only reduced when neither
  immortal nor immune.

Net original semantics: the attack is fully blocked iff an immune type (`prot < 0`) is reached
*before* the first positively-protected type (`prot >= 1`) in ascending index order (a `prot==0`
type does not "break" immune-candidacy). When **not** fully blocked, an immune type still adds
`raw + |prot|` to the total.

## OpenGothic file:line
`/Users/admin/Downloads/opengothic/game/game/damagecalculator.cpp`
- `swordDamage` G2 loop: lines 183-196 (esp. 193 `if(other.protection[i]>=0)`)
- `swordDamage` G1 loop: lines 205-211
- `rangeDamage(Damage)`: lines 133-145 (esp. 139)
- `checkDamageMask` bullet branch: line 232

## Divergence
OpenGothic uses a per-type *filter*:
```
int vd = std::max(raw - other.protection[i], 0);
if(other.protection[i] >= 0) { value += vd; invincible = false; }
```
so (a) an immune type (`protection[i] < 0`) contributes **nothing** to `value` and never flips
`invincible`, and (b) `invincible` (=> total 0, and the non-spell MinDamage floor is skipped)
is true **iff every active type is immune**.

The original instead (a) **always** accumulates `max(raw - prot, 0)` — including immune types,
which add `raw + |prot|` — and (b) decides the full zero-out via the order-dependent
immune-candidate rule above, where `protection[i] == 0` is treated differently from `>= 0`.

Observable differences (multi-bit masks only):
- Victim immune to a *higher-index* type but positively protected on a *lower-index* type
  (e.g. EDGE=2 protected, MAGIC=5 immune): OG ignores the magic component; the original adds
  `raw_magic + 1` on top of the physical damage.
- Victim immune to a *lower-index* type and protected on a *higher-index* type (e.g. BLUNT=1
  immune, EDGE=2 protected): the original zeroes the **entire** attack (immune flag set before
  the positive type), whereas OG deals the edge damage. A type with `protection == 0` does not
  rescue the victim in the original but does count as "not immune" in OG.

## Proposed patch
**DEFERRED.**

Reasons:
1. **Negligible vanilla impact.** Every divergence requires a damage descriptor with two or
   more active `damagetype` bits combined with a per-type immunity on the victim. Vanilla G2
   weapons and spells are single-type, and for any single active type OG and the original agree
   exactly (verified: immune → both 0; `prot==0` → both deal `raw`; positive prot → both deal
   `max(raw-prot,0)` and both apply the MinDamage floor). So a fix changes no vanilla outcome.
2. **Non-surgical + regression risk.** Matching the original would require changing the
   accumulation/`invincible` computation in four places (`swordDamage` G1+G2, `rangeDamage`,
   `checkDamageMask`) and faithfully reproducing the order-dependent immune-candidate rule
   (including the `prot==0` vs `prot>=1` boundary and the "immune before first positive type"
   ordering). Getting this subtly wrong risks regressing the common single-type path for no
   vanilla benefit. Per "empty beats false positives," deferring until a concrete multi-type
   repro (e.g. a specific mod) justifies the rewrite.

If implemented later, the loop should mirror (NOTE citation to include):
`// NOTE: in original-game oCNpc::OnDamage_Hit @0x00666610 the protection loop accumulates`
`// max(raw-protection[i],0) for EVERY active type (immune types add raw+|prot|), and zeroes`
`// the whole total only when an immune type (protection<0) precedes the first type with`
`// protection>=1 in ascending index order; protection==0 does not break immune-candidacy.`
