# Npc_GetPortalGuild returns 0 (GIL_NONE) instead of -1 on the invalid-handle path

**Confidence:** Medium (divergence confirmed against the binary and the fix is a surgical one-line init change; the trigger — an invalid `self`/NPC handle — is uncommon in shipping scripts, so impact is narrow).

## Original fn + address

The `Npc_GetPortalGuild` handler is `FUN_006e4ee0` (Gothic2.exe, `oGameExternal.cpp`). It
initializes its result accumulator to **-1**, parses the NPC argument via the usual
`zCParser`/`FUN_006db090` instance fetch, and only overwrites the accumulator when the NPC is
non-null AND `oCGame::GetPortalRoomManager(ogame)` is non-null, calling
`oCPortalRoomManager::GetCurNpcPortalRoomGuild` (@0x00773160). That helper itself returns **-1**
whenever the NPC is not currently inside a guild-owned portal room (null arg, no sector,
unregistered sector). The handler then unconditionally `zCParser::SetReturn`s the accumulator.
Net result: a null/invalid NPC handle (and the "no portal-room manager" case) yields **-1**.

By contrast, the player-side `oCPortalRoomManager::GetCurPlayerPortalRoomGuild` (@0x00772ff0)
returns **0** on its no-room path — so `Wld_GetPlayerPortalGuild`/`Wld_GetFormerPlayerPortalGuild`
legitimately default to 0 (GIL_NONE), but the NPC external does not. OpenGothic's player externals
(`wld_getplayerportalguild`, gamescript.cpp:1733) already match at 0; only the NPC external is off.

## OG file:line

`game/game/gamescript.cpp:2786-2792` — `GameScript::npc_getportalguild`:

```cpp
int GameScript::npc_getportalguild(std::shared_ptr<zenkit::INpc> npcRef) {
  int32_t g   = GIL_NONE;          // GIL_NONE == 0  (game/game/constants.h:9)
  auto    npc = findNpc(npcRef);
  if(npc!=nullptr)
    g = world().guildOfRoom(npc->portalName());
  return g;
  }
```

## Divergence

On the invalid-handle / "no portal-room manager" path the original returns **-1**, but OpenGothic
returns **GIL_NONE (0)** because the accumulator is seeded with `GIL_NONE`. `GIL_NONE` is a *valid*
guild id (0), not a sentinel, so a script using the result as a "no guild / unreachable" marker
(e.g. `if(Npc_GetPortalGuild(x) == GIL_NONE)` or as an array index) reads "guild 0" where the
original reports "none / -1". (For a *valid* NPC outside any portal, OG already matches: a non-portal
NPC has an empty `portalName()`, and `World::guildOfRoom(std::string_view)` at world.cpp:1076 returns
-1 for a name without ':'. So the divergence is isolated to the seed value on the null path.)

This is the same null-handle sentinel-parity class already corrected for `Npc_GetHeightToNpc`
(gamescript.cpp:2503, fixed to INT_MAX) and `Npc_GetActiveSpellCat/Level` (fixed to -1).

## Proposed patch

OLD (`game/game/gamescript.cpp:2786-2792`):
```cpp
int GameScript::npc_getportalguild(std::shared_ptr<zenkit::INpc> npcRef) {
  int32_t g   = GIL_NONE;
  auto    npc = findNpc(npcRef);
  if(npc!=nullptr)
    g = world().guildOfRoom(npc->portalName());
  return g;
  }
```

NEW:
```cpp
int GameScript::npc_getportalguild(std::shared_ptr<zenkit::INpc> npcRef) {
  // NOTE: in original-game Npc_GetPortalGuild (Gothic2.exe oGameExternal.cpp FUN_006e4ee0) seeds the
  // return with -1 and only overwrites it via oCPortalRoomManager::GetCurNpcPortalRoomGuild
  // (@0x00773160) for a valid NPC inside a guild-owned portal room; an invalid handle (or missing
  // portal-room manager) returns -1 -- unlike the player getter (@0x00772ff0) which defaults to 0.
  // OpenGothic seeded with GIL_NONE(0), a valid guild id, so a null self read "guild 0" not "none".
  int32_t g   = -1;
  auto    npc = findNpc(npcRef);
  if(npc!=nullptr)
    g = world().guildOfRoom(npc->portalName());
  return g;
  }
```

(One-line seed change; the valid-NPC branch is unchanged. `GIL_NONE` is defined `= 0` in
`game/game/constants.h:9`; `World::guildOfRoom` already returns -1 for a non-portal name.)
