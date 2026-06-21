# Fall damage missing +50cm tolerance offset

**Confidence:** High

## Original function + address
`oCNpc::CreateFallDamage(float fallDist)` @ `0x00681da0` (Gothic2.exe, `_roman/oNpc.cpp`).
Called from `oCAniCtrl_Human::CheckFallStates` @ `0x006b5810` (xref `0x006b60ce`), which
passes the AI-player's tracked vertical fall distance (member offset `+0x9c`, in cm).

In prose, the original computes the inflicted hit-points as:

    damage = ( fallDist + 50.0 - fallDownHeight ) * 0.01 * fallDownDamage

i.e. a **constant +50.0 cm tolerance is added to the fallen distance** *before* the
`falldown_height` guild threshold is subtracted, and the result is scaled by `0.01`
(= divide by 100) and by `falldown_damage`. `fallDownHeight` is the per-guild
`falldown_height`, `fallDownDamage` the per-guild `falldown_damage` (default `+1.0` mul).

## OG location
`game/game/damagecalculator.cpp:38` — `DamageCalculator::damageFall`, specifically the
value computed at line 51:

    ret.value = int32_t(dmgPerMeter*(height-h0)/100.f - float(prot));

where `height = 0.5*|g|*(speed/g)^2` (the reconstructed fall distance, equivalent to the
original's tracked `fallDist`), `h0 = falldown_height`, `dmgPerMeter = falldown_damage`.

## Divergence
OG uses `(height - h0)`. The original uses `(fallDist + 50 - h0)`. OG is missing the
constant **+50cm** added to the fall distance. Net effect: OG under-counts the effective
fall distance by 50cm on **every** fall, so the player/NPCs take less fall damage than the
original (and falls that should just barely hurt deal zero in OG). This is a flat,
unambiguous numeric divergence, gameplay-visible at thresholds.

## Proposed patch

File: `game/game/damagecalculator.cpp`

OLD:
```cpp
  float   gravity     = DynamicWorld::gravity;
  float   fallTime    = speed/gravity;
  float   height      = 0.5f*std::abs(gravity)*fallTime*fallTime;
  float   h0          = float(g.falldown_height[gl]);
```
NEW:
```cpp
  float   gravity     = DynamicWorld::gravity;
  float   fallTime    = speed/gravity;
  // NOTE: in original-game oCNpc::CreateFallDamage (0x00681da0) a constant +50cm
  // tolerance is added to the fallen distance before subtracting falldown_height:
  //   dmg = (fallDist + 50 - falldown_height) * 0.01 * falldown_damage
  float   height      = 0.5f*std::abs(gravity)*fallTime*fallTime + 50.f;
  float   h0          = float(g.falldown_height[gl]);
```

The subsequent `dmgPerMeter*(height-h0)/100.f` then matches the original
`(fallDist + 50 - h0) * 0.01 * falldown_damage` exactly.
