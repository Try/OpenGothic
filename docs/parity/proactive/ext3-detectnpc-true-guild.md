# Wld_DetectNpcEx / Wld_DetectNpc guild filter matches live guild, not TRUE guild

**Confidence:** High

## Original function + address

- `Wld_DetectNpcEx` handler (Gothic2.exe `oGameExternal.cpp`, FUN_006e15c0) calls
  `oCNpc::FindNpcEx` @0x00740b80.
- `Wld_DetectNpc` handler (FUN_006e11d0) calls `oCNpc::FindNpc` @0x00740a80.
- In BOTH `FindNpcEx` and `FindNpc`, the per-candidate guild filter is, in prose:
  `guildParam < 0  ||  (signed char)candidate[0x766] == guildParam`. Offset `0x766` is the
  **true-guild** byte: it is exactly the field returned by `oCNpc::GetTrueGuild` @0x00730770
  (`return (int)(char)this[0x766]`) and written by `oCNpc::SetTrueGuild` @0x00730780. The live,
  script-mutable guild is a *different* field — `oCNpc::GetGuild` @0x00730750 returns
  `*(int*)(this+0x230)` — and is NOT what the detect filter consults.

So both world-detect externals select candidates by their **true guild**, never by the live
`C_Npc.guild`.

## OpenGothic file:line

- `game/game/gamescript.cpp:1883` — `GameScript::wld_detectnpcex`
- `game/game/gamescript.cpp:1858` — `GameScript::wld_detectnpc` (same bug, same line shape)

## Divergence

Both handlers filter with `int32_t(n.guild())==guild`. `Npc::guild()`
(`game/world/objects/npc.cpp:1314`) returns `hnpc->guild` — the live guild (engine field
`0x230`). The original filters on the **true** guild (field `0x766`, i.e. `Npc::trueGuild()`,
`game/world/objects/npc.cpp:1350`). When an NPC's live guild has been changed at runtime
(disguise / transform / runtime guild swap) so that `guild() != trueGuild()`, OpenGothic detects
the wrong set of NPCs: it matches on the disguised live guild instead of the permanent one the
engine actually keys on.

This is the same field confusion already corrected for `IsMonster`/`IsHuman` under issue #656
(see the NOTE at `game/world/objects/npc.cpp:1322-1324`), which established `trueGuild()` as the
OG mapping of the original's `0x766` field. The existing NOTE on `wld_detectnpcex`
(`gamescript.cpp:1884`) covers only the alive/conscious flag, not this guild divergence — so this
finding is not in the README "Applied" table.

`Npc::trueGuild()` is grep-verified to exist (declared `game/world/objects/npc.h:226`, defined
`game/world/objects/npc.cpp:1350`) and returns `int32_t`.

## Proposed patch

`game/game/gamescript.cpp` — `wld_detectnpcex` (line 1883):

```cpp
// OLD
       (guild==-1 || int32_t(n.guild())==guild) &&
// NEW
       // NOTE: in original-game oCNpc::FindNpcEx @0x00740b80 (the Wld_DetectNpcEx handler) the guild
       // filter compares the TRUE guild (field 0x766, oCNpc::GetTrueGuild @0x00730770), NOT the live
       // script-mutable C_Npc.guild (field 0x230, oCNpc::GetGuild @0x00730750). A disguised NPC must
       // be detected by its permanent guild. Same field distinction as IsMonster/IsHuman (#656).
       (guild==-1 || n.trueGuild()==guild) &&
```

`game/game/gamescript.cpp` — `wld_detectnpc` (line 1858), same fix citing
`oCNpc::FindNpc @0x00740a80`:

```cpp
// OLD
       (guild==-1 || int32_t(n.guild())==guild) &&
// NEW
       // NOTE: in original-game oCNpc::FindNpc @0x00740a80 (the Wld_DetectNpc handler) the guild
       // filter compares the TRUE guild (field 0x766), not the live C_Npc.guild (field 0x230). (#656)
       (guild==-1 || n.trueGuild()==guild) &&
```

Build-safe: `Npc::trueGuild()` returns `int32_t`, compared against `int guild` — no cast needed
(the old `int32_t(...)` wrapper around the `uint32_t` `guild()` is dropped).
