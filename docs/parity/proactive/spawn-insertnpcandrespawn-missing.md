# Wld_InsertNpcAndRespawn external not implemented

> DEFER: full parity needs a respawn-timer subsystem (new); binding just the insertion is a partial feature. Defer pending the respawn design.

Confidence: Medium

## Original function

`Wld_InsertNpcAndRespawn` external (`oGameExternal.cpp`, FUN_006e0190 @ 0x006e0190).
Daedalus signature: `Wld_InsertNpcAndRespawn(npcInstance, spawnpoint, spawnDelay)`.

Decompiled behavior:
- Pops 3 params: spawnDelay (int, popped first), spawnpoint (string), npcInstance (int).
- Calls `oCSpawnManager::SpawnNpc(npcInstance, spawnpoint, 0.0)` — same spawn path
  as `Wld_InsertNpc` (places the NPC at the named waypoint).
- On success it additionally:
  - writes `spawnDelay` into the NPC's respawn-timer field (offset 0x27c),
  - sets the NPC's respawn flag bit (`flags |= 0x10` at offset 0x75c),
  - calls `oCRtnManager::UpdateSingleRoutine` to (re)start its routine.
- On failure it logs "Wld_InsertNpcAndRespawn(): npc could not be spawned ...".

The respawn flag + delay let the spawn manager re-insert the monster `spawnDelay`
seconds after it dies — the standard mechanism for regenerating wildlife/monsters
in Gothic 2 (NotR).

## OpenGothic

No binding exists. `game/game/gamescript.cpp` registers `wld_insertnpc`,
`wld_removenpc`, `wld_spawnnpcrange`, `wld_isnextfpavailable` but not
`wld_insertnpcandrespawn`. A search of `game/` for `respawn`/`Respawn`/`spawnTime`
returns nothing — there is no respawn-timer field or spawn-manager equivalent.

At runtime any script call to `Wld_InsertNpcAndRespawn` falls through to
`Gothic::notImplementedRoutine` (`game/gothic.cpp:964`,1007): it logs once and
does nothing, so the NPC is **never inserted** (unlike `Wld_InsertNpc`, which
spawns it).

## Divergence (gameplay)

Scripts that use `Wld_InsertNpcAndRespawn` to place a monster get no NPC at all
in OG (vs. an inserted, respawning monster in the original). Respawn-on-death is
entirely absent.

## Proposed patch (partial — restores insertion, not full respawn)

A full fix needs a respawn timer in `Npc`/`WorldObjects`. The minimal parity step
is to at least insert the NPC like `Wld_InsertNpc` does:

```
// game/game/gamescript.cpp  (near wld_insertnpc)
// NEW
void GameScript::wld_insertnpcandrespawn(int npcInstance, std::string_view spawnpoint, float spawnDelay) {
  // NOTE: in original-game (oGameExternal.cpp Wld_InsertNpcAndRespawn) this spawns
  // the NPC at the waypoint exactly like Wld_InsertNpc, then stores spawnDelay as a
  // respawn timer and sets a respawn flag so the spawn manager re-inserts the NPC
  // spawnDelay seconds after it dies. Respawn-on-death is not yet modeled here.
  (void)spawnDelay;
  if(npcInstance<=0)
    return;
  auto npc = world().addNpc(size_t(npcInstance),spawnpoint);
  if(npc!=nullptr)
    fixNpcPosition(*npc,0,0);
  }

// game/game/gamescript.cpp  (bind list, after wld_insertnpc)
// OLD
  bindExternal("wld_insertnpc",                  &GameScript::wld_insertnpc);
// NEW
  bindExternal("wld_insertnpc",                  &GameScript::wld_insertnpc);
  bindExternal("wld_insertnpcandrespawn",        &GameScript::wld_insertnpcandrespawn);
```

(Plus the matching declaration in `gamescript.h`.) Full parity additionally
requires re-spawning the NPC `spawnDelay`s after death.
