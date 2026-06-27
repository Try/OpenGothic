# AI-state-driven NPCs are wrongly teleported to spawn on Wld_SetTime / savegame load

**Confidence:** High

## Original function + address

`oCRtnManager::SetDailyRoutinePos(int param)` (Gothic2.exe `0x7764d0`). It walks the
global NPC list and, for each NPC, only re-places it onto its daily-routine position when
`param == 0` **OR** `oCNpc_States::IsAIStateDriven` returns 0. In prose: when `param != 0`
the routine manager *skips* every "AI-state-driven" NPC, leaving it exactly where it is and
not clearing its currently running state. It then calls `RestartRoutines`, which for
AI-state-driven NPCs only re-activates the state via `StartRtnState(0)` — and that call is a
no-op when the state is already running (`ActivateRtnState` returns early because the
state-active flag is set), so position and state are effectively untouched.

`oCNpc_States::IsAIStateDriven` (`0x76e8c0`) returns true exactly when the NPC has **no daily
routine list** *and* `start_aistate > 0` — i.e. monsters / spawned creatures that run purely
on a `start_aistate` rather than a TA daily routine.

Callers fix `param`:
- `oCGame::SetTime` (`0x6c4de0`, the Wld_SetTime / wait / sleep path) calls it with `param = 1`.
- `oCGame::ChangeLevel` (`0x6c7290`) calls it with `param = (slot != SAVEGAME_SLOT_NEW)` — so
  `param = 0` only on a brand-new game, and `param = 1` on every savegame load / level revisit.

Net behavior: AI-state-driven monsters are repositioned to their routine/spawn waypoint **only
on a fresh new game**. On wait/sleep (`Wld_SetTime`) and on load/level-change they keep their
live position and state.

## OpenGothic file:line

- `game/world/worldobjects.cpp:972` `WorldObjects::resetPositionToTA()` (the OG analogue of
  `SetDailyRoutinePos`) resets **every** NPC unconditionally — there is no AI-state-driven skip
  and no `param`.
- For an AI-state-driven monster `Npc::resetPositionToTA()` (`game/world/objects/npc.cpp:479`)
  takes `currentTaPoint()` which, with `routines.empty()`, resolves to `findPoint(hnpc->wp)`
  (the spawn waypoint), then `setPosition(...)`, `clearAiQueue()` and `clearState(true)` — i.e.
  it teleports the monster back to spawn and tears down its running state.
- Callers that must distinguish new-game from load/wait:
  `game/world/world.cpp:400` (`World::setDayTime`, the Wld_SetTime path) and
  `game/game/gamesession.cpp:508` (`GameSession::initScripts(firstTime)`; `firstTime` already
  equals "new game", matching `slot != SAVEGAME_SLOT_NEW`).

## Divergence

In OpenGothic, every time the player waits/sleeps (`Wld_SetTime`) or loads a savegame / revisits
a level, all monsters that run on a `start_aistate` (no daily routine) are teleported back to
their spawn waypoint and have their AI state cleared and restarted. The original explicitly
exempts exactly those NPCs from the reset in all of those cases, repositioning them only on a
brand-new game. Result: in OG a wandering/relocated monster snaps home on every wait or load;
in the original it stays put.

## Proposed patch

Thread the original `param==0` (new-game) flag through and skip AI-state-driven NPCs otherwise.

1. New accessor in `game/world/objects/npc.h` (near the existing routine helpers, line ~485) and
   `npc.cpp`. Grep-verified `isAiStateDriven` does not already exist; `routines` (npc.h:619) and
   `hnpc->start_aistate` (used npc.cpp:3072) both exist.

```
// NOTE: in original-game oCNpc_States::IsAIStateDriven @0x76e8c0 an NPC is "AI-state-driven"
// when it has no daily routine list and a positive start_aistate (typ. spawned monsters).
bool Npc::isAiStateDriven() const {
  return routines.empty() && hnpc->start_aistate!=0;
  }
```
(declaration `bool isAiStateDriven() const;` in npc.h)

2. `game/world/worldobjects.h:138` / `world.h:108`: change signature to take the flag:
```
OLD: void resetPositionToTA();
NEW: void resetPositionToTA(bool fullReset);
```

3. `game/world/worldobjects.cpp:972` `WorldObjects::resetPositionToTA(bool fullReset)` — inside
   the per-NPC loop at line 986, skip AI-state-driven NPCs when `!fullReset`:
```
  for(size_t i=0;i<npcArr.size();) {
    auto& n = *npcArr[i];
    // NOTE: in original-game oCRtnManager::SetDailyRoutinePos @0x7764d0, when called for a
    // non-new-game reset (Wld_SetTime/oCGame::SetTime, level-change & savegame load ->
    // param!=0) AI-state-driven NPCs are skipped entirely: they keep their position and the
    // state restart in RestartRoutines is a no-op. Only a fresh new game (param==0) resets them.
    if(!fullReset && n.isAiStateDriven()) {
      ++i;
      continue;
      }
    if(n.resetPositionToTA()){
      ...unchanged...
```

4. `game/world/world.cpp:295` wrapper `World::resetPositionToTA(bool fullReset)` forwarding
   `wobj.resetPositionToTA(fullReset)`; `game/world/world.cpp:400` `setDayTime` ->
   `wobj.resetPositionToTA(false)`; `game/game/gamesession.cpp:508` `initScripts` ->
   `wrld->resetPositionToTA(firstTime)`.

(Optional/strictly-faithful: the unconditional `i->attachToPoint(nullptr)` loop at
worldobjects.cpp:982 also touches AI-state-driven NPCs that the original leaves alone; left out
of the surgical fix because it is not the visible divergence and detaching is harmless for the
no-routine case.)
