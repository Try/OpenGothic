# Talent: ACROBAT does not raise fall-damage threshold

**Confidence:** High

## Original function + address

`oCNpc::SetTalentSkill(int talent, int level)` at `0x00730f60` (Gothic2.exe).

When the talent being set is the ACROBAT talent (index `0x0B`), and the level
actually changes, the function also modifies the NPC's *per-NPC* fall-down
height (stored at object offset `0x90c`, read through the getter at `0x00681790`):

- When the level *increases* (new > old): it sets fall-down height to
  `currentFallDownHeight * 2.0` (the acrobatic overlay `HUMANS_ACROBATIC.MDS`
  is also applied and `InitAnimations` re-run).
- When the level *decreases* (new < old): it sets fall-down height to
  `currentFallDownHeight * 0.5`.

`oCNpc::CreateFallDamage` (`0x00681da0`) computes fall damage from this
per-NPC fall-down height at `0x90c` (used when the default flag at `0x908` is 0),
not from the raw guild value. So learning acrobatics roughly doubles the height
a character can fall from before taking damage. Since acrobat is a binary 0/1
talent, the net effect when learned is `guildFalldownHeight * 2`.

## OpenGothic location

- `game/world/objects/npc.cpp:1178-1182` — `invalidateTalentOverlays`: for
  `TALENT_ACROBAT` only the `_ACROBATIC.MDS` overlay is added/removed; the
  fall-down height is never touched.
- `game/game/movealgo.cpp:643-646` — `MoveAlgo::falldownHeight()` returns only
  the guild value `falldown_height[gl]`.
- `game/game/damagecalculator.cpp:42-66` — `DamageCalculator::damageFall` uses
  only the guild value `g.falldown_height[gl]`.

There is no per-NPC fall-down height and no acrobat multiplier anywhere in
OpenGothic (`grep` for `acrobat`/`falldown` confirms). Result: the ACROBAT
talent gives the animation overlay but provides **no protection from fall
damage**, diverging from the original where it doubles the safe fall height.

## Divergence

Acrobatic NPCs/players take fall damage at the same height as non-acrobatic ones
in OpenGothic; in the original they can fall ~2x as far unharmed.

## Proposed patch

Apply the acrobat factor where the safe fall height is consumed. Two call
sites read it; gate both on `talentSkill(TALENT_ACROBAT)`.

`game/game/damagecalculator.cpp`

OLD:
```cpp
  float   h0          = float(g.falldown_height[gl]);
  float   dmgPerMeter = float(g.falldown_damage[gl]);
```
NEW:
```cpp
  // NOTE: in original-game oCNpc::SetTalentSkill (0x00730f60) the ACROBAT talent
  // doubles the per-NPC fall-down height (offset 0x90c) consumed by
  // CreateFallDamage (0x00681da0). Mirror that 2x safe-fall threshold here.
  float   h0          = float(g.falldown_height[gl]);
  if(npc.talentSkill(TALENT_ACROBAT)>0)
    h0 *= 2.f;
  float   dmgPerMeter = float(g.falldown_damage[gl]);
```

`game/game/movealgo.cpp`

OLD:
```cpp
float MoveAlgo::falldownHeight() const {
  auto gl = npc.guild();
  return float(npc.world().script().guildVal().falldown_height[gl]);
  }
```
NEW:
```cpp
float MoveAlgo::falldownHeight() const {
  auto  gl = npc.guild();
  float h0 = float(npc.world().script().guildVal().falldown_height[gl]);
  // NOTE: in original-game the ACROBAT talent doubles the per-NPC fall-down
  // height (oCNpc::SetTalentSkill 0x00730f60); keep the gravity-trigger height
  // consistent with the damage threshold above.
  if(npc.talentSkill(TALENT_ACROBAT)>0)
    h0 *= 2.f;
  return h0;
  }
```

Note: `TALENT_ACROBAT` is in `game/game/constants.h`; ensure it is in scope in
both translation units (npc.h is already included transitively).
