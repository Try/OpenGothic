# isMonster()/isHuman() key off live guild instead of true guild

**Confidence:** High

## Original fn + addr (prose)

`oCNpc::IsHuman` (Gothic2.exe, 0x742640) returns simply whether the
single-byte field at object offset **0x766** is `< 0x11` (17 == GIL_SEPERATOR_HUM).
`oCNpc::IsMonster` (0x742600) first checks two virtual predicates (the death /
unconscious vtable slots) and, if both are false, returns true when the same
**0x766** field is not one of 0x2f / 0x28 / 0x29 (47/40/41 ==
GIL_DRAGON / GIL_FIREGOLEM / GIL_ICEGOLEM).

The decisive point: offset 0x766 is the **true guild**. `oCNpc::GetTrueGuild`
(0x730770) returns `(char)this[0x766]`, `oCNpc::SetTrueGuild` (0x730780) writes
it, and `oCNpc::IsGuildFriendly` (0x7307a0) also reads 0x766. The **live**
guild is a separate 4-byte field at offset 0x230 (`oCNpc::GetGuild` 0x730750 /
`oCNpc::SetGuild` 0x730760) — this is the one scripts mutate via `C_Npc.guild`
and the fake-guild/disguise system. So in the original, monster/human
classification is driven by the *true* guild, immune to runtime `C_Npc.guild`
reassignment.

## OG current file:line

- `game/world/objects/npc.cpp:1295` `isMonster()` tests `guild()`
- `game/world/objects/npc.cpp:1302` `isHuman()` tests `guild()`
- `game/world/objects/npc.cpp:1291` `guild()` returns the clamped **live**
  `hnpc->guild` (the script-mutable symbol)
- `game/world/objects/npc.cpp:1312` `trueGuild()` already exists and is seeded
  at init (`npc.cpp:199 setTrueGuild(hnpc->guild)`), matching the original's
  init of 0x766.

## Divergence

OpenGothic's `isMonster()`/`isHuman()` read the **live** guild; the original
reads the **true** guild. Whenever a script changes `C_Npc.guild` at runtime
(disguise / fake-guild logic, or any mod that reassigns guild) the two engines
disagree on monster/human status. This gates real behavior — death-vs-knockout
floor (`npc.cpp:563-565`), weapon auto-equip (`game/game/inventory.cpp:964`),
inventory-menu access (`ui/inventorymenu.cpp:110`), block calc
(`npc.cpp:2040`), damage talent path (`damagecalculator.cpp:167`). Note OG is
already internally inconsistent: `isTargetableBySpell` (`npc.cpp:3170`) reads
`trueGuild()` for the orc/undead tests but then calls `isHuman()` which reads
the live guild — the original uses true guild for both.

This is exactly the #656 hypothesis ("isMonster keys off live guild, changing
C_Npc.guild has no effect in vanilla"), now confirmed at the decompilation
level: vanilla reads 0x766 (true guild), not 0x230 (live guild).

(The threshold itself matches: original `< 0x11` vs OG `< GIL_SEPERATOR_HUM`
== `< 16`; guild 16 is the unused SEPERATOR marker so the field choice, not the
boundary, is the bug.)

## Proposed patch

```cpp
// game/world/objects/npc.cpp

// --- OLD ---
bool Npc::isMonster() const {
  const bool g2 = owner.version().game==2;
  const auto SEPERATOR_ORC = g2 ? GIL_SEPERATOR_ORC : GIL_G1_SEPERATOR_ORC;
  const auto SEPERATOR_HUM = g2 ? GIL_SEPERATOR_HUM : GIL_G1_SEPERATOR_HUM;
  return SEPERATOR_HUM<guild() && guild()<SEPERATOR_ORC;
  }

bool Npc::isHuman() const {
  const bool g2 = owner.version().game==2;
  const auto SEPERATOR_HUM = g2 ? GIL_SEPERATOR_HUM : GIL_G1_SEPERATOR_HUM;
  return guild() < SEPERATOR_HUM;
  }

// --- NEW ---
bool Npc::isMonster() const {
  // NOTE: in original-game oCNpc::IsMonster (0x742600) / IsHuman (0x742640)
  // classify on the TRUE guild (object field 0x766), not the live, script-
  // mutable guild (field 0x230). Mutating C_Npc.guild must not change
  // monster/human status.
  const bool g2 = owner.version().game==2;
  const auto SEPERATOR_ORC = g2 ? GIL_SEPERATOR_ORC : GIL_G1_SEPERATOR_ORC;
  const auto SEPERATOR_HUM = g2 ? GIL_SEPERATOR_HUM : GIL_G1_SEPERATOR_HUM;
  const auto tg = uint32_t(trueGuild());
  return SEPERATOR_HUM<tg && tg<SEPERATOR_ORC;
  }

bool Npc::isHuman() const {
  // NOTE: in original-game oCNpc::IsHuman keys off the TRUE guild (field 0x766).
  const bool g2 = owner.version().game==2;
  const auto SEPERATOR_HUM = g2 ? GIL_SEPERATOR_HUM : GIL_G1_SEPERATOR_HUM;
  return uint32_t(trueGuild()) < SEPERATOR_HUM;
  }
```
