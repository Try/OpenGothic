# Issue #791 — Npc full-day simulation

## Issue
When the player neither skips time nor advances the story, NPC daily routines do not always converge over multiple in-game days. Several NPCs are named:
- **Zuris** — script routine cycles through stand/guard/sit/sleep at different waypoints; the NPC fails to switch back because the script requires the NPC be within ~5m of the way-point to take the routine action ("Not switching back, as script requires 5m distance from way-point").
- **Halvor** — suspected same root cause.
- **Knight**, **Barracks** — similar non-convergence (screenshots).

## OG files
- game/world/worldobjects.cpp: routine state stepping `WorldObjects::tickRoutines`/state apply (around worldobjects.cpp:180-200, 990-1010), `resetPositionToTA` worldobjects.cpp:966.
- game/world/objects/npc.cpp: `Npc::resetPositionToTA` npc.cpp:476, `currentTaPoint`, `attachToPoint`, routine entry refresh `invokeRefreshAtInsert`.
- game/game/gamescript.cpp: `MoveAlgo::isClose(*npc,*w,MAX_AI_USE_DISTANCE)` waypoint-proximity test gamescript.cpp:2301; routine-driven `AI_GotoWP`/`B_*` script callbacks.

## Original behavior (prose)
In the original, when an NPC's routine state changes (TA boundary), the NPC actively walks to the new state's start waypoint and only then performs the scripted action; the script's proximity check (be near the way-point) is satisfied because the engine paths the NPC there as part of entering the new routine state. Across full real-time days the routine state machine steps each NPC through its schedule even without player time-skips, so positions converge.

## OG current file:line
- Routine state change applied at worldobjects.cpp:185-187 (`if(s!=i.curState) { ... i.curState = s; }`); per-NPC reset/insert at worldobjects.cpp:997-1005 and npc.cpp:476.
- Script proximity gate: gamescript.cpp:2301.

## Divergence
On a routine-state transition the NPC is not reliably routed (walked) to the new state's waypoint before the scripted action runs, so the `isClose(... MAX_AI_USE_DISTANCE)` / 5m check fails and the NPC stays at the previous spot — the routine never "switches back," producing non-convergence over a simulated full day. This is a runtime state-machine / pathing-convergence problem spanning the routine ticker and AI goto handling, not a one-line gate.

## Recommendation: DEFER
Too broad for a confident surgical patch; needs runtime reproduction. Guide:
1. Repro one NPC (Zuris) without time-skips; log every routine `curState` transition (worldobjects.cpp:185) and the subsequent script callback + `isClose`/`MAX_AI_USE_DISTANCE` result (gamescript.cpp:2301).
2. Confirm whether, on transition, an `AI_GotoWP`/path-to-waypoint is enqueued and completes before the state's action runs. The likely gap: routine-state entry applies the new state without first walking the NPC to the new waypoint when the NPC is already idle far from it.
3. Compare against original by stepping the routine-manager entry path; ensure an active goto-to-waypoint is issued on state entry so the proximity gate is met.
4. Validate convergence across several simulated days for Zuris, Halvor, and the Knight/Barracks cases before landing.
