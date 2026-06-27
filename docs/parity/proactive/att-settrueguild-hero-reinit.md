# Npc_SetTrueGuild on the hero does not re-initialize NPC attitudes

**Confidence:** High

## Original function + address

`Npc_SetTrueGuild` external handler (`Gothic2.exe` @ `0x006ee660`, source
`oGameExternal.cpp`) resolves the target NPC, calls `oCNpc::SetTrueGuild`, and
then performs a player-only side effect: it tests the NPC's "is-the-player"
virtual (vtbl slot `0x100`) and, **when the NPC is the player**, calls
`oCGame::InitNpcAttitudes` (`@0x006c61d0`, `oGame.cpp`).

`oCGame::InitNpcAttitudes` walks the whole world NPC list and, for every NPC
that is not the player, recomputes the guild-matrix attitude between that NPC's
**true guild** and the player's **true guild**
(`oCGuilds::GetAttitude(npc.GetTrueGuild(), player.GetTrueGuild())`, guilds
object `@0x00700d40`) and writes that value into **both** the NPC's temp
attitude (`oCNpc::SetTmpAttitude`) and perm attitude (`oCNpc::SetAttitude`).
In other words: changing the hero's true guild re-bakes every NPC's standing
toward the hero from the guild table, discarding any per-NPC temp/perm attitude.

## OpenGothic file:line

`game/game/gamescript.cpp:2823` — `GameScript::npc_settrueguild`:

```cpp
int GameScript::npc_settrueguild(std::shared_ptr<zenkit::INpc> npcRef, int gil) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    npc->setTrueGuild(gil);
  return 0;
  }
```

`Npc::setTrueGuild` (`game/world/objects/npc.cpp:1336`) only stores `trGuild = g`
and touches nothing else.

## Divergence

OpenGothic stores the new true guild and stops. It never re-initializes the
other NPCs' attitudes, so calling `Npc_SetTrueGuild(hero, NEW_GUILD)` (the script
mechanism used when the player joins a guild/camp) leaves every NPC's standing
toward the hero stale:

- NPCs that had been explicitly angered/befriended (perm or temp attitude set
  via `Npc_SetAttitude` / `Npc_SetTempAttitude`) keep that attitude instead of
  being reset to the new guild-matrix value.
- More generally, the whole world does not re-evaluate friend/foe relative to
  the hero's new guild at the moment of the guild change.

This is a missing side effect, distinct from the already-fixed
`GetAttitude(hero)`/`personAttitude` temp-vs-perm, guild-table FRIENDLY default,
and `disguise_guild` items.

## Proposed patch

Add a faithful port of `InitNpcAttitudes`, invoked only when the affected NPC is
the player. All referenced symbols are grep-verified: `GameSession::player()`
returns `Npc*` (`game/game/gamesession.h:53`); `GameScript::world()` returns
`World&` (`game/game/gamescript.cpp:817`); `World::npcCount()`/`World::npcById`
(`game/world/world.h:48-49`) iterate every NPC (same pattern as
`game/game/gamescript.cpp:756` and `game/game/playercontrol.cpp:389`);
`Npc::isPlayer()` (`game/world/objects/npc.h:116`);
`Npc::setAttitude`/`Npc::setTempAttitude` (`game/world/objects/npc.cpp:1370,1380`);
`GameScript::guildAttitude(const Npc&,const Npc&)` (`game/game/gamescript.h:166`).

OLD (`game/game/gamescript.cpp:2823`):

```cpp
int GameScript::npc_settrueguild(std::shared_ptr<zenkit::INpc> npcRef, int gil) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    npc->setTrueGuild(gil);
  return 0;
  }
```

NEW:

```cpp
int GameScript::npc_settrueguild(std::shared_ptr<zenkit::INpc> npcRef, int gil) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr) {
    npc->setTrueGuild(gil);
    // NOTE: in original-game Npc_SetTrueGuild @0x006ee660 calls oCGame::InitNpcAttitudes
    // @0x006c61d0 when the affected NPC is the player: every other NPC's perm AND temp
    // attitude is re-baked from the guild matrix (oCGuilds::GetAttitude with both parties'
    // true guilds). OpenGothic only stored the guild, so changing the hero's guild left
    // every NPC's standing toward the hero stale (angered/befriended NPCs were never reset).
    if(npc->isPlayer()) {
      auto* pl = owner.player();
      auto& w  = world();
      if(pl!=nullptr) {
        for(uint32_t i=0; i<w.npcCount(); ++i) {
          Npc* n = w.npcById(i);
          if(n==nullptr || n==pl)
            continue;
          Attitude att = guildAttitude(*n,*pl);
          n->setAttitude(att);
          n->setTempAttitude(att);
          }
        }
      }
    }
  return 0;
  }
```

Note: the original keys the guild lookup on `GetTrueGuild()` for both parties,
whereas `GameScript::guildAttitude` reads the live `Npc::guild()`. The two agree
for every non-disguised NPC; if strict true-guild parity for disguised NPCs is
desired, the lookup should be expressed over `trueGuild()` instead. The reused
`guildAttitude` keeps the patch consistent with OpenGothic's existing
guild-matrix convention and is the lower-risk choice.
