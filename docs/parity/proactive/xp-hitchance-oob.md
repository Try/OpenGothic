# Npc::hitChance off-by-one OOB read (Picklock talent row)

**Confidence:** Medium

## Original function + address

`oCNpc::GetTalentInfo` (Gothic2.exe `0x0073c860`) and the stats-screen path
read per-talent display data. The hit-chance array on `oCNpc` has exactly five
slots, addressed by the combat talent indices only (unknown / 1H / 2H / bow /
crossbow). Non-combat talents such as PICKLOCK (index 5) are never indexed into
the hit-chance array; their stats row shows the talent skill value, and any
hit-chance accessor in the original bounds-checks strictly below the array
length (`index < 5`). There is no slot at index 5.

## OpenGothic location

`game/world/objects/npc.cpp:1209`

```
int32_t Npc::hitChance(Talent t) const {
  if(t<=zenkit::INpc::hitchance_count)   // hitchance_count == 5
    return hnpc->hitchance[t];
  return 0;
  }
```

`zenkit::INpc::hitchance` is a 5-element array (`hitchance_count == 5`, valid
indices 0..4) — see `lib/ZenKit/include/zenkit/addon/daedalus.hh:179,192`.

## Divergence

The guard uses `<=` instead of `<`. For `t == hitchance_count (== 5)` the
condition is true and the code reads `hnpc->hitchance[5]`, one element past the
end of the array (out-of-bounds read).

This is reachable from gameplay: the stats menu loops every talent index and,
in Gothic 2, calls `pl.hitChance(Talent(i))` for each
(`game/ui/gamemenu.cpp:1248-1254`, `i = 0 .. TALENT_MAX_G2-1`). `TALENT_PICKLOCK
== 5` (`game/game/constants.h:442`). So opening the character/stats screen on
any save where the Picklock talent row is populated reads garbage memory and
prints a bogus `"<garbage>%"` value for that row. Indices 6..21 correctly fall
through to `return 0`; only index 5 is mishandled.

In the original game the Picklock row never displays a hit-chance percentage.

## Proposed patch

File: `game/world/objects/npc.cpp`

OLD:
```
int32_t Npc::hitChance(Talent t) const {
  if(t<=zenkit::INpc::hitchance_count)
    return hnpc->hitchance[t];
  return 0;
  }
```

NEW:
```
int32_t Npc::hitChance(Talent t) const {
  // NOTE: in original-game the hit-chance array on oCNpc has exactly
  // hitchance_count (5) slots, addressed only by combat talents (1H/2H/bow/
  // crossbow). Index 5 (TALENT_PICKLOCK) and above are not hit-chance talents.
  // The bound must be strict (< count); `<=` reads one element past the array.
  if(t<zenkit::INpc::hitchance_count)
    return hnpc->hitchance[t];
  return 0;
  }
```
