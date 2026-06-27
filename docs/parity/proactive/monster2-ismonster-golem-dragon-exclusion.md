# Monster classification: Fire/Ice Golem and Dragon wrongly counted as monsters

**Confidence:** High

## Original function + address

`oCNpc::IsMonster` (Gothic2.exe `0x00742600`, virtual). The original decides monster
status as a chain, not a single range test:

1. If the NPC `IsAPlayer` (`0x007425a0`, `this==player`) -> not a monster.
2. Else if the NPC `IsHuman` (`0x00742640`, `trueGuild < 0x11` i.e. `< GIL_SEPERATOR_HUM`)
   -> not a monster.
3. Else it IS a monster **only if** the true guild (field `+0x766`) is **not** one of the
   three explicitly excluded boss/creature guilds: `0x28` (40 = `GIL_FIREGOLEM`),
   `0x29` (41 = `GIL_ICEGOLEM`), `0x2f` (47 = `GIL_DRAGON`).

So in the original a Fire Golem, Ice Golem, and Dragon are classified as **non-monsters**
(these are the talk/boss-capable creatures in G2 NotR). Note the original also has **no
upper `GIL_SEPERATOR_ORC` bound** - orcs fall through to monster status - which is a second,
separate divergence flagged below.

## OpenGothic file:line

`game/world/objects/npc.cpp:1301` (`Npc::isMonster`), specifically line 1308:

```
return SEPERATOR_HUM<trueGuild() && trueGuild()<SEPERATOR_ORC;
```

## Divergence

OpenGothic classifies monsterhood with a single half-open range
`GIL_SEPERATOR_HUM(16) < trueGuild < GIL_SEPERATOR_ORC(58)`. That range **includes**
`GIL_FIREGOLEM(40)`, `GIL_ICEGOLEM(41)` and `GIL_DRAGON(47)`, so OpenGothic treats Fire
Golems, Ice Golems and Dragons as monsters while the original does not.

`isMonster()` is load-bearing for at least two combat paths, so the misclassification is
behavioral, not cosmetic:
- `game/game/damagecalculator.cpp:172` - "regular monsters always do critical damage" when
  attacking unarmed (`tal==TALENT_UNKNOWN`, `critChance=-1`). Dragons/golems attack unarmed,
  so in OpenGothic they auto-crit (full, undivided damage) whereas the original runs the
  normal `hitchance` roll for them.
- `game/world/objects/npc.cpp:566` - `minHp = isMonster() ? 0 : 1`, which feeds the
  death-vs-unconscious threshold; the original gives these three guilds the non-monster
  `minHp=1` path.

## Proposed patch

Surgical: keep OpenGothic's existing range gate (so the riskier orc-bound difference is left
untouched, see note) and add the original's three-guild G2 exclusion.

OLD (`game/world/objects/npc.cpp:1305-1308`):
```cpp
  // NOTE: in original-game oCNpc::IsMonster/IsHuman (Gothic2.exe 0x742600/0x742640) classify on
  // the TRUE guild (field 0x766), not the live script-mutable C_Npc.guild; OG read the live
  // guild, so a runtime guild change (disguise) wrongly flipped monster/human status. (#656)
  return SEPERATOR_HUM<trueGuild() && trueGuild()<SEPERATOR_ORC;
```

NEW:
```cpp
  // NOTE: in original-game oCNpc::IsMonster/IsHuman (Gothic2.exe 0x742600/0x742640) classify on
  // the TRUE guild (field 0x766), not the live script-mutable C_Npc.guild; OG read the live
  // guild, so a runtime guild change (disguise) wrongly flipped monster/human status. (#656)
  if(!(SEPERATOR_HUM<trueGuild() && trueGuild()<SEPERATOR_ORC))
    return false;
  // NOTE: in original-game oCNpc::IsMonster (Gothic2.exe 0x00742600) additionally excludes
  // GIL_FIREGOLEM(0x28), GIL_ICEGOLEM(0x29) and GIL_DRAGON(0x2f) from monster classification;
  // these talk/boss-capable creatures are NOT monsters (they must not auto-crit or take the
  // monster minHp=0 death path).
  if(g2) {
    const auto tg = trueGuild();
    if(tg==GIL_FIREGOLEM || tg==GIL_ICEGOLEM || tg==GIL_DRAGON)
      return false;
    }
  return true;
```

Grep-verified symbols: `GIL_FIREGOLEM`/`GIL_ICEGOLEM`/`GIL_DRAGON` exist in
`game/game/constants.h` (40/41/47); `trueGuild()`, `g2`, `SEPERATOR_HUM`, `SEPERATOR_ORC`
already in scope in this function. Only the G2 branch is changed; G1 (no G1 decompile
available) keeps the existing range test.

### Secondary divergence (NOT patched here)

The original `IsMonster` has no `GIL_SEPERATOR_ORC` upper bound, so orcs (guild >= 59) are
monsters in the original but non-monsters in OpenGothic (which clamps at `SEPERATOR_ORC`).
Removing OpenGothic's upper bound would also flip orc death-handling (`minHp`) and unarmed
auto-crit, a broader change with unclear secondary effects, so it is left **DEFERRED**
pending a dedicated orc-behavior parity pass rather than folded into this surgical fix.
