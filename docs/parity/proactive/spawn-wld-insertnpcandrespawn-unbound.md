# Wld_InsertNpcAndRespawn is unbound — silent no-op leaves NPC unspawned and corrupts the Daedalus stack

**Confidence:** High (for the "external is unbound / NPC never spawns" core fact). The
respawn-timer half is DEFERRED.

## Original function + address

The Daedalus external `Wld_InsertNpcAndRespawn` is handled in the original by the external
thunk at `FUN_006e0190` (registered via `DefineExternals_Ulfi`,
`P:\dev\g2addon\release\Gothic\_ulf\oGameExternal.cpp`). It reads three parameters from the
parser in this order: an `int` spawn-delay, the spawnpoint `zSTRING`, and the `int` npc
instance index. The Daedalus call signature is therefore
`Wld_InsertNpcAndRespawn(int npcInstance, string spawnpoint, int spawnDelay)`.

It then calls `oCSpawnManager::SpawnNpc` (`@00778ba0`) with (npc, spawnpoint, 0.0) exactly
like the plain `Wld_InsertNpc` handler (`FUN_006df1f0`). On success it differs from
`Wld_InsertNpc` in two ways:
- it stores the spawn-delay into the npc (field at `+0x27c`), and
- it **sets** the respawn flag `npc.flags |= 0x10` (the plain `Wld_InsertNpc` handler instead
  **clears** it: `npc.flags &= ~0x10`),
then, if the game is running, calls `oCRtnManager::UpdateSingleRoutine`. The `0x10` flag is
later consulted by the spawn manager's remove/respawn path (`oCSpawnManager::CheckRemoveNpc`
`@007792e0` and friends) to re-insert the NPC after it has been culled for distance.

## OpenGothic file:line

`game/game/gamescript.cpp:118` — the bind table registers `wld_insertnpc` but there is **no**
`bindExternal("wld_insertnpcandrespawn", ...)` anywhere (grep-verified: the only insert binding
in `game/` is `wld_insertnpc`).

Unbound externals are routed to the default handler at `game/gothic.cpp:964` →
`Gothic::notImplementedRoutine` (`game/gothic.cpp:1007`), which only logs
`not implemented call [...]` once and returns.

## Divergence

When a Gothic II script calls `Wld_InsertNpcAndRespawn` (used by mods and some vanilla spawn
scripts), OpenGothic:
1. **does not spawn the NPC at all** (the original spawns it at the waypoint), and
2. routes through the default-external no-op, which neither pops the three pushed arguments
   off the Daedalus data stack nor pushes a result — leaving the VM stack unbalanced for the
   continuation of that script function.

The original instead places the NPC at the spawnpoint waypoint and enables the respawn flag.

## Proposed patch

High-confidence core fix: bind the external and spawn the NPC (consuming the 3 args so the VM
stack stays balanced), mirroring `wld_insertnpc`. The argument order matches the original
(`npcInstance, spawnpoint, spawnDelay`).

The `spawn_delay` field already exists on the handle
(`lib/ZenKit/include/zenkit/addon/daedalus.hh:207` `var int32_t spawn_delay;`), so the delay
can be recorded for a future respawn implementation.

OLD (`game/game/gamescript.cpp`, bind table near line 118):
```cpp
  bindExternal("wld_insertnpc",                  &GameScript::wld_insertnpc);
```
NEW:
```cpp
  bindExternal("wld_insertnpc",                  &GameScript::wld_insertnpc);
  bindExternal("wld_insertnpcandrespawn",        &GameScript::wld_insertnpcandrespawn);
```

OLD (`game/game/gamescript.cpp`, after `wld_insertnpc` at ~line 2036):
```cpp
void GameScript::wld_insertnpc(int npcInstance, std::string_view spawnpoint) {
  if(npcInstance<=0)
    return;

  auto npc = world().addNpc(size_t(npcInstance),spawnpoint);
  if(npc!=nullptr)
    fixNpcPosition(*npc,0,0);
  }
```
NEW (add the new function immediately after):
```cpp
void GameScript::wld_insertnpc(int npcInstance, std::string_view spawnpoint) {
  if(npcInstance<=0)
    return;

  auto npc = world().addNpc(size_t(npcInstance),spawnpoint);
  if(npc!=nullptr)
    fixNpcPosition(*npc,0,0);
  }

void GameScript::wld_insertnpcandrespawn(int npcInstance, std::string_view spawnpoint, int spawnDelay) {
  // NOTE: in original-game Wld_InsertNpcAndRespawn handler FUN_006e0190 spawns the NPC at the
  // spawnpoint (oCSpawnManager::SpawnNpc @00778ba0) exactly like Wld_InsertNpc, stores spawnDelay
  // (npc+0x27c), and sets the respawn flag (npc.flags |= 0x10). The respawn timer/cull-and-reinsert
  // half is DEFERRED (see below); this binding at least spawns the NPC and balances the VM stack
  // instead of the current default-external no-op.
  if(npcInstance<=0)
    return;

  auto npc = world().addNpc(size_t(npcInstance),spawnpoint);
  if(npc!=nullptr) {
    npc->handlePtr()->spawn_delay = spawnDelay;
    fixNpcPosition(*npc,0,0);
    }
  }
```

And the matching declaration in `game/game/gamescript.h` next to `wld_insertnpc` (line 242):
```cpp
    void wld_insertnpc       (int npcInstance, std::string_view spawnpoint);
    void wld_insertnpcandrespawn(int npcInstance, std::string_view spawnpoint, int spawnDelay);
```

(grep-verify before applying: `Npc::handlePtr()` exists and returns the `zenkit::INpc` handle;
confirm the exact `addNpc` overload `addNpc(size_t, std::string_view)` is used by `wld_insertnpc`
— it is, at `game/game/gamescript.cpp:2033`.)

**DEFERRED:** the actual respawn behavior — culling the NPC when the player leaves
SPAWN_REMOVERANGE and re-inserting it after `spawn_delay` when the player returns
(original `oCSpawnManager` delayed-spawn list + `CheckRemoveNpc`/`CheckInsertNpc`). OpenGothic
has no equivalent spawn-manager cull/respawn loop, so reproducing the timer is a separate,
larger feature, not a surgical change.
