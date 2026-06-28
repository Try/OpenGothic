# Npc_GetGuildAttitude: unbound external returns ATT_HOSTILE instead of the guild matrix value

**Confidence:** High (exact mechanism; all backing state already exists in OG;
genuine wrong-default). Medium only on how frequently base-G2/mod scripts call it.

## Original fn + address

The external `Npc_GetGuildAttitude` is registered in `oCGame::DefineExternals_Ulfi`
with handler `FUN_006e87a0`-class wrapper at **0x006e5c00** (params: `self`, `other`;
return `int`). The wrapper pops both NPC instances and computes:

- `g_other = oCNpc::GetTrueGuild(other)` (**0x00730770** — returns the true-guild
  byte at object field `+0x766`), then
- `oCNpc::GetGuildAttitude(self, g_other)` (**0x007307d0**), which reads `self`'s own
  true-guild byte (same field `+0x766`) and returns
  `oCGuilds::GetAttitude(self.trueGuild, other.trueGuild)` (the guild matrix, i.e.
  OG's `gilAttitudes`).

So the original answer is purely `gilAttitudes[ self.trueGuild ][ other.trueGuild ]`,
keyed on **trueGuild for BOTH parties** (field 0x766 = `Npc::trueGuild()`), never the
live `C_Npc.guild`. Note the handler only `SetReturn`s when both instances are
non-null; with a null arg the parser default (0 = `ATT_HOSTILE`) stands.

## OG file:line

`game/game/gamescript.cpp` — `Npc_GetGuildAttitude` is **never** registered in the
`bindExternal(...)` table (verified: no `getguildattitude`/`GetGuildAttitude` symbol
anywhere except the unrelated `wld_getguildattitude`). Per
`docs/parity/proactive/ext-unbound-externals-inventory.md`, an unbound `func int`
silently returns `0` via ZenKit's default external — and `0 == ATT_HOSTILE`.

## Divergence

Any script call `Npc_GetGuildAttitude(a, b)` returns **0 (ATT_HOSTILE)** in OG
regardless of the actual guild standing, instead of the matrix value
(commonly `ATT_FRIENDLY=3`/`ATT_NEUTRAL=2`). The needed state already exists:
`Npc::trueGuild()` (`game/world/objects/npc.h:226`), the `gilAttitudes` matrix and
`gilCount` members, and the `ATT_*` enum — the identical lookup is already performed
inline at `gamescript.cpp:2934-2936` (the `Npc_SetTrueGuild` player re-bake) and in
`guildAttitude()` at `gamescript.cpp:1423`. Note `guildAttitude()` keys on the *live*
`guild()`, so it must NOT be reused here: the original keys on `trueGuild()`, which
diverges for disguised NPCs (armor/disguise changes live guild but not trueGuild).

## Proposed patch

OLD (`bindExternal` table, near `gamescript.cpp:129`):
```cpp
  bindExternal("wld_getguildattitude",           &GameScript::wld_getguildattitude);
```
NEW:
```cpp
  bindExternal("wld_getguildattitude",           &GameScript::wld_getguildattitude);
  bindExternal("npc_getguildattitude",           &GameScript::npc_getguildattitude);
```

OLD (new method body, alongside `npc_getpermattitude`, ~`gamescript.cpp:2984`):
```cpp
```
NEW:
```cpp
int GameScript::npc_getguildattitude(std::shared_ptr<zenkit::INpc> aRef, std::shared_ptr<zenkit::INpc> bRef) {
  auto a = findNpc(aRef);
  auto b = findNpc(bRef);
  // NOTE: in original-game Npc_GetGuildAttitude @0x006e5c00 returns
  // oCGuilds::GetAttitude(self.GetTrueGuild(), other.GetTrueGuild()) -- the guild matrix
  // keyed on the TRUE guild (field 0x766) of BOTH parties, never the live C_Npc.guild
  // (so guildAttitude(), which uses live guild(), is deliberately not reused for disguises).
  // Same row/col order (self*gilCount + other) as the InitNpcAttitudes re-bake at line 2936.
  if(a==nullptr || b==nullptr)
    return ATT_NEUTRAL;
  auto aG = std::min<size_t>(gilCount-1, size_t(a->trueGuild()));
  auto bG = std::min<size_t>(gilCount-1, size_t(b->trueGuild()));
  return gilAttitudes[aG*gilCount+bG];
  }
```

Plus the header declaration in `game/game/gamescript.h` next to line 357:
```cpp
    int  npc_getguildattitude(std::shared_ptr<zenkit::INpc> aRef, std::shared_ptr<zenkit::INpc> bRef);
```

Caveat: the original leaves the parser default (0/ATT_HOSTILE) on a null arg; this
patch follows OG's prevailing convention (`npc_getattitude` returns `ATT_NEUTRAL` on
null) for the degenerate case, which differs only when a script passes an invalid NPC.
