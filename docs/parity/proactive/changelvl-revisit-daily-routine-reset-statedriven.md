# Level re-entry resets *all* NPCs to their daily routine, dropping the "AI-state-driven" exemption

**Confidence:** Medium-High (divergence is unambiguous in the decompile; gameplay impact is real but niche). Patch is **DEFERRED** — no faithful, surgical mapping exists in OpenGothic's current NPC AI model without risking the shared `resetPositionToTA` reset path.

## Original function + address (prose only)

- `oCGame::ChangeLevel` @ `0x006c7290` (the level-change driver, reached from the `oCTriggerChangeLevel` touch path). After loading the destination world it calls the routine manager once.
- `oCRtnManager::SetDailyRoutinePos` @ `0x007764d0`. It takes a single integer argument. `ChangeLevel` passes the value `(loadedSlot != SAVEGAME_SLOT_NEW)` — i.e. **0 when the destination world is brand-new (loaded fresh from its `.zen`), and 1 when the world is being re-entered (loaded back from the per-world temp savegame).**
- Inside `SetDailyRoutinePos`, for every NPC in the world it conditionally snaps the NPC to its current-time daily-routine waypoint, then calls `RestartRoutines`. The condition is: snap the NPC **only if** `arg == 0` **OR** the NPC is **not** "AI-state-driven". The predicate is `oCNpc_States::IsAIStateDriven` @ `0x0076e8c0`, which reports true when the NPC is currently executing an active, non-routine scripted AI state (a state pushed outside the passive daily routine — e.g. combat/flee/one-shot `ZS_*` state).
- Net behavior: on a **new** world every NPC is repositioned to its routine (arg 0 forces it); on **re-entry** to a previously-visited world, NPCs that are mid AI-state are left exactly as they were saved (position + state preserved), and only the idle/routine NPCs are re-snapped to their current-time routine waypoint.

## OpenGothic file:line

- `game/game/gamesession.cpp:406-407` — `initScripts(wss.isEmpty()); wrld->triggerOnStart(wss.isEmpty());` (`wss.isEmpty()` is OG's "first time / new world" flag; non-empty = re-entry).
- `game/game/gamesession.cpp:508` — `wrld->resetPositionToTA();` at the tail of `GameSession::initScripts`, executed **unconditionally** for both the new-world and the re-entry path (the `firstTime` argument is not consulted here).
- `game/world/worldobjects.cpp:972` — `WorldObjects::resetPositionToTA()`, which for **every** NPC in `npcArr` calls `Npc::resetPositionToTA()` (`game/world/objects/npc.cpp:479`). That per-NPC routine unconditionally `clearState(true)` + `clearAiQueue()` + snaps to the current TA waypoint for any living, non-player NPC with a TA point — there is no "is this NPC currently AI-state-driven, skip it" guard.

## Divergence

On **re-entry to a previously-visited level**, OpenGothic resets *all* NPCs to their daily-routine positions and **wipes their active AI state**, whereas the original game preserves NPCs that are AI-state-driven (leaving their saved position and in-progress scripted state intact) and only re-snaps the idle/routine NPCs. Concretely: an NPC that was in an active, non-routine state (combat, flee, a self-managed `ZS_*` state, a pursuing monster) at the moment you left a world will, on your return, be teleported to its routine waypoint with its state cleared under OpenGothic; under the original it would still be where/how you left it. The new-world path is faithful (both reset everyone), so the bug is specific to the revisit branch.

## Proposed patch — DEFERRED

A faithful fix must (a) thread the "is this a re-entry (`!firstTime` / `!wss.isEmpty()`)" flag from `GameSession::initScripts` down into `World::resetPositionToTA` → `WorldObjects::resetPositionToTA`, and (b) in the per-NPC loop, skip the reset for NPCs that are currently AI-state-driven (the analogue of `oCNpc_States::IsAIStateDriven`). Reasons for deferral:

1. **No 1:1 OG predicate.** OpenGothic has no direct equivalent of `IsAIStateDriven`. The closest fields are `Npc::aiState.funcIni` (`game/world/objects/npc.h:615`, current AI-state function) and the routine query `Npc::isInRoutine(...)`, but the original's exact condition (`routine-flag == 0 AND active-AI-state-depth > 0`) does not map cleanly, and `isAiBusy()`/`isAiQueueEmpty()` (`npc.cpp:4633/4639`) describe queued actions, not "driven by a pushed non-routine state". Picking the wrong predicate risks either still wiping state-driven NPCs (no fix) or wrongly skipping idle NPCs that should re-snap (new regression).
2. **Shared reset path.** `WorldObjects::resetPositionToTA` is reused by `World::setDayTime`/`Wld_SetTime` (`game/world/world.cpp:394-401`) where resetting *all* NPCs is correct. The exemption applies only to changelevel-revisit, so the flag must be plumbed without altering the default (reset-all) behavior of those callers.
3. **Reset side effects.** OG's `WorldObjects::resetPositionToTA` also detaches every NPC from its point and churns the `npcInvalid` list up-front (`worldobjects.cpp:976-983`); a correct skip must guard those pre-loop steps too, not just the per-NPC snap, so it is not a one-line change.

A surgical patch should not be attempted until a verified "AI-state-driven" predicate exists on `Npc`.

<!--
NOTE: in original-game oCGame::ChangeLevel @0x006c7290 calls oCRtnManager::SetDailyRoutinePos
@0x007764d0 with arg=(loadedSlot!=SAVEGAME_SLOT_NEW); that routine snaps an NPC to its daily-routine
waypoint only when arg==0 OR oCNpc_States::IsAIStateDriven @0x0076e8c0 is false, preserving
state-driven NPCs on level re-entry. OpenGothic GameSession::initScripts (game/game/gamesession.cpp:508)
calls World::resetPositionToTA unconditionally on every load, which resets all NPCs and clears their
AI state regardless of the firstTime/revisit flag.
-->
