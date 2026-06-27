# tg-settrueguild-reinit-uses-live-guild

Npc_SetTrueGuild attitude re-bake reads the LIVE guild via `guildAttitude`, but the
original `oCGame::InitNpcAttitudes` keys the matrix on the TRUE guild of both parties.

**Confidence:** High

## Original function + address
- `Npc_SetTrueGuild` (Gothic2.exe `FUN_006ee660`, the script external) calls
  `oCNpc::SetTrueGuild` (sets only the true-guild byte at oCNpc+0x766) and, when the
  affected NPC is the player, calls `oCGame::InitNpcAttitudes` @ `0x006c61d0`.
- `oCGame::InitNpcAttitudes` @ `0x006c61d0` walks every NPC `n != player` and sets both its
  perm and temp attitude to `oCGuilds::GetAttitude(GetTrueGuild(n), GetTrueGuild(player))` —
  i.e. it reads the TRUE guild (field 0x766, `GetTrueGuild` @0x00730770) of BOTH the witness
  NPC and the player, never the live `C_Npc.guild` (field 0x230, `GetGuild` @0x00730750).
  The same true-guild re-bake is performed inline by `oCGame::ChangeLevel` @0x006c7290 on a
  new game (`GetTrueGuild` for both parties).
- Critically, `oCNpc::SetTrueGuild` mutates only the true-guild field; the live guild is left
  unchanged. So `InitNpcAttitudes` deliberately reads the freshly-set TRUE guild, which can
  differ from the (stale/disguised) live guild.

## OpenGothic file:line
`game/game/gamescript.cpp:2912` (inside `GameScript::npc_settrueguild`, the reinit loop):

```cpp
Attitude att = guildAttitude(*n,*pl);
n->setAttitude(att);
n->setTempAttitude(att);
```

`GameScript::guildAttitude` (gamescript.cpp:1422) indexes the matrix with the LIVE guild:
`std::min<size_t>(gilCount-1, p0.guild())` / `...p1.guild()`. And `Npc::setTrueGuild`
(npc.cpp:1346) sets only `trGuild`; it does NOT touch `hnpc->guild`, so right after the call
`pl->guild()` still returns the OLD live guild while `pl->trueGuild()` holds the new value.

## Divergence
The reinit is supposed to mirror `InitNpcAttitudes`, which reads the TRUE guild of both the
witness NPC and the player. OpenGothic instead routes through `guildAttitude`, which reads the
LIVE guild of both. Two concrete failures:
1. Player side: because `setTrueGuild` never updates the live guild, the just-set new guild is
   ignored — the matrix is read with the player's stale live guild, so the whole re-bake can be
   a no-op or compute the pre-change relations (exactly what the engine path exists to avoid).
2. Witness side: a disguised NPC `n` (disguise_guild applied → live guild = disguise, true guild
   = real) is re-baked from its disguise guild instead of its permanent guild.

This is the same live-vs-true field distinction as IsMonster/IsHuman (#656) and
Wld_DetectNpc(Ex), now in the attitude-init path. (Note: this is NOT the general combat
hostility oracle — `guildAttitude`/`personAttitude` correctly stay on the live guild for
disguise to work; only this `InitNpcAttitudes` emulation must use the true guild.)

## Proposed patch
Replicate `guildAttitude`'s matrix lookup but on the TRUE guilds, matching `InitNpcAttitudes`.
`gilAttitudes` and `gilCount` are `GameScript` members (gamescript.h:490-491) and are in scope
here; `Npc::trueGuild()` is grep-verified (npc.h:226, npc.cpp:1350).

OLD (gamescript.cpp:2912):
```cpp
          Attitude att = guildAttitude(*n,*pl);
          n->setAttitude(att);
          n->setTempAttitude(att);
```

NEW:
```cpp
          // NOTE: in original-game Npc_SetTrueGuild @0x006ee660 the player branch calls
          // oCGame::InitNpcAttitudes @0x006c61d0, which keys the guild matrix on the TRUE guild
          // (GetTrueGuild, field 0x766) of BOTH parties -- not the live C_Npc.guild. SetTrueGuild
          // leaves the live guild untouched, so guildAttitude() (live) re-baked from the stale
          // pre-change guild; read trueGuild() like InitNpcAttitudes does.
          auto selfG = std::min<size_t>(gilCount-1, size_t(n->trueGuild()));
          auto plG   = std::min<size_t>(gilCount-1, size_t(pl->trueGuild()));
          Attitude att = Attitude(gilAttitudes[selfG*gilCount+plG]);
          n->setAttitude(att);
          n->setTempAttitude(att);
```

Row/column order is unchanged from the existing `guildAttitude(*n,*pl)` call (witness row,
player column), so only the guild field changes from live to true; build-verifiable, no new
symbols.
