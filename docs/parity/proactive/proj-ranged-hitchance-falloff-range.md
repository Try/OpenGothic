# Ranged hit-chance distance falloff uses wrong reference/max ranges (1500/4500 vs original 1000/10000)

**Confidence:** High

## Original function + address

`oCAIArrow::ReportCollisionToAI(zCCollisionReport const&)` @ **0x006a18a2** (Gothic2.exe,
`P:\dev\g2addon\release\Gothic\_carsten\oVisFX.h` / arrow AI).

This is the routine that fires when an arrow/bolt projectile (`zCCollObjectProjectile`)
collides with something. When the thing hit is an NPC, it decides whether the shot
actually *lands* damage via a distance-based hit-chance falloff:

1. It resolves the shooter NPC (the arrow's owner, stored at arrow offset `+0x5c`) and
   the target NPC (the character pulled out of the collision report).
2. It computes a **straight-line distance** between the shooter's world position
   (`zMAT4::GetTranslation` of the shooter's vob matrix at `+0x3c`) and the target NPC's
   world position. Call this `dist`.
3. It fetches the shooter's ranged hit chance with `oCNpc::GetHitChance(shooter, type)`,
   where `type` is `3` for crossbow (`GetWeaponMode()==5`) and `4` for bow.
4. It interpolates the effective hit chance against two range bounds and the bow talent:
   below the near bound the chance trends toward ~100%, between the two bounds it falls
   off toward `0`, and beyond the far bound it is effectively `0`.
5. The two range bounds are read **once and cached** (`DAT_00aada44` guard) from the
   Daedalus parser symbols `RANGED_CHANCE_MINDIST` and `RANGED_CHANCE_MAXDIST`
   (via a parser-symbol lookup at `FUN_007938d0`). Stock Gothic II scripts do **not**
   define these symbols, so the binary's baked-in defaults are used:
   * `RANGED_CHANCE_MINDIST` default = **1000.0** (10 m) — the near/reference range
     (`_DAT_008b1158`)
   * `RANGED_CHANCE_MAXDIST` default = **10000.0** (100 m) — the far/max range
     (`_DAT_008b115c`)
6. Finally it rolls `rand()%100 < effectiveChance` to apply or skip the damage message.

There is **no** separate hard range cutoff at ~4500 in this routine; the only outer
bound is `MAXDIST` (10000), past which the interpolated chance has already reached 0.
(The bullet itself is independently retired by flight code at a ~10000cm path length,
which corroborates 10000 as the intended outer range.)

## OpenGothic file:line

`game/game/damagecalculator.cpp:73` — `DamageCalculator::rangeDamage(Npc& nsrc, Npc& nother, const Bullet& b, ...)`,
specifically the falloff block at lines **84-99**, with the range constants defined in
`game/game/constants.h:133-139`:

```
ReferenceBowRangeG1 = 2000,
ReferenceBowRangeG2 = 1500,
MaxBowRange         = 4500,
```

```cpp
float refRange  = g2 ? ReferenceBowRangeG2 : ReferenceBowRangeG1;   // 1500 in G2
float maxRange  = float(MaxBowRange);                                // 4500
float chance    = b.hitChance();

if(dist<refRange)
  hitCh = mix(1.f, chance, (dist / refRange));
else if(dist<maxRange)
  hitCh = mix(chance, 0.f, (dist-refRange) / (maxRange-refRange));
else
  hitCh = 0;
noHit = (dist>float(MaxBowRange) || hitCh<=hitChance);
```

The interpolation *shape* matches the original. Two things diverge:

* **Reference (near) range:** OG uses `1500`; original default is `1000`.
* **Max (far) range:** OG uses `4500`; original default is `10000`.

(`ReferenceBowRangeG2`/`MaxBowRange` are also reused as AI *engagement* ranges in
`game/game/fightalgo.cpp:375,387`, so they must not be repurposed in place — the fix
introduces dedicated falloff constants.)

## Divergence

Gameplay-visible difference in ranged combat hit probability vs. the original:

* Inside ~10-15 m the crossover point where the chance stops being near-100% and starts
  decaying is shifted (1500 vs 1000), so mid-close shots land slightly differently.
* The big one: OG drives the hit chance to **0 at 45 m** and treats anything past 45 m as
  a guaranteed miss (`dist>MaxBowRange`), while the original keeps a non-zero,
  linearly-decaying chance all the way out to **100 m**. In the original you can reliably
  snipe distant targets with a high bow/crossbow talent; in OpenGothic those same shots
  silently deal no damage past ~45 m and decay to near-nothing much sooner. This makes
  long-range archery markedly weaker than on `Gothic2.exe`.

## Proposed patch

Add dedicated falloff constants (matching the original's baked-in `RANGED_CHANCE_MINDIST`
/ `RANGED_CHANCE_MAXDIST` defaults) and use them only in the ranged-damage falloff, leaving
the AI-engagement `ReferenceBowRangeG2`/`MaxBowRange` untouched.

`game/game/constants.h` — OLD:

```cpp
enum {
  ReferenceBowRangeG1 = 2000,
  ReferenceBowRangeG2 = 1500,
  MaxBowRange         = 4500,
  MaxMagRange         = 3500, // from Focus_Ranged
  MaxFightRange       = 4500,
  };
```

NEW:

```cpp
enum {
  ReferenceBowRangeG1 = 2000,
  ReferenceBowRangeG2 = 1500,
  MaxBowRange         = 4500,
  MaxMagRange         = 3500, // from Focus_Ranged
  MaxFightRange       = 4500,
  };

enum {
  // NOTE: in original-game oCAIArrow::ReportCollisionToAI @0x006a18a2 the ranged
  // hit-chance distance falloff reads RANGED_CHANCE_MINDIST / RANGED_CHANCE_MAXDIST
  // from Daedalus; stock G2 scripts omit them, so the binary defaults apply:
  // near/reference range 1000cm (_DAT_008b1158), far/max range 10000cm (_DAT_008b115c).
  RangedChanceMinDist = 1000,
  RangedChanceMaxDist = 10000,
  };
```

`game/game/damagecalculator.cpp` (in `rangeDamage`, ~lines 87-99) — OLD:

```cpp
    bool  g2        = Gothic::inst().version().game==2;
    float refRange  = g2 ? ReferenceBowRangeG2 : ReferenceBowRangeG1;
    float maxRange  = float(MaxBowRange);
    float chance    = b.hitChance();

    if(dist<refRange)
      hitCh = mix(1.f, chance, (dist / refRange));
    else if(dist<maxRange)
      hitCh = mix(chance, 0.f, (dist-refRange) / (maxRange-refRange));
    else
      hitCh = 0;

    noHit = (dist>float(MaxBowRange) || hitCh<=hitChance);
```

NEW:

```cpp
    bool  g2        = Gothic::inst().version().game==2;
    // NOTE: in original-game oCAIArrow::ReportCollisionToAI @0x006a18a2 the falloff uses
    // RANGED_CHANCE_MINDIST(1000)/RANGED_CHANCE_MAXDIST(10000) defaults, not the bow
    // AI-engagement ranges; chance decays to 0 only at 10000cm (no hard cutoff at 4500cm).
    (void)g2;
    float refRange  = float(RangedChanceMinDist);
    float maxRange  = float(RangedChanceMaxDist);
    float chance    = b.hitChance();

    if(dist<refRange)
      hitCh = mix(1.f, chance, (dist / refRange));
    else if(dist<maxRange)
      hitCh = mix(chance, 0.f, (dist-refRange) / (maxRange-refRange));
    else
      hitCh = 0;

    noHit = (dist>maxRange || hitCh<=hitChance);
```

(If keeping the `g2` local is preferred over `(void)g2;`, the `bool g2` line can simply be
dropped — it is not used elsewhere in this block.)

### Secondary (DEFERRED) — distance metric

The original measures **straight-line shooter→target distance** at impact; OpenGothic uses
`b.pathLength()` (accumulated arrow flight distance). These agree closely for flat shots and
both share the 10000cm outer bound, so it is not the headline bug. Switching to
`nsrc.position().distance(nother.position())` is plausible (both NPCs are in scope as
`nsrc`/`nother`), but it is left **DEFERRED**: `dist` is also reused for the spell
path-length cutoff just above (line 79), and changing the metric there would need separate
verification against the spell collision path. The range-constant fix above is the
high-confidence, surgical change.
