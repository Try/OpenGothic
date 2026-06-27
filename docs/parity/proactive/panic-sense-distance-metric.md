# Panic/monster subsystem sweep — sensing-distance metric (NPC flee / monster aggro)

**Confidence:** NO HIGH-CONFIDENCE FINDING (one low-confidence DEFERRED candidate below).

## Scope swept
Scared-NPC flee-from-fight, prey/scavenger flee, ZS_MM_Rtn wander, monster aggro
range, herd/flock, timid run-on-sight, monster sleep-until-disturbed / wake-on-approach.

## Negative results (verified, no divergence)
- **Active/passive perception ranges** — `GameScript::PerDist::at` (game/game/gamescript.cpp:80)
  returns the per-perception `PERC_DIST` override when `>0`, else falls back to `senses_range`.
  Matches the original table (default -1 → senses_range). `passivePerceptionProcess`
  (game/world/worldobjects.cpp:949) and `perceptionProcess` (game/world/objects/npc.cpp:4545)
  both compare squared distance against `range*range`. Consistent.
- **Smell ignores LoS / through-walls aggro** — OpenGothic `canSenseNpc`
  (game/world/objects/npc.cpp:5032) sets `SENSE_SMELL` purely on range (no raycast),
  matching original `oCNpc::CanSense` @0x00740740 whose smell branch returns success on
  `GetDistanceToVobApprox <= senses_range` with no line-of-sight test.
- **Sight capped at senses_range** — original `oCNpc::CanSee` @0x00741c10 compares its
  octagonal length approximation against `senses_range` (this+0x284); OpenGothic caps sight
  at the same `senses_range`. Consistent.
- **AI near/far processing radii** 3000/6000 (game/world/worldobjects.cpp:219-220) and
  flee-waypoint search radius `5*100` (npc.cpp:2021) — these are OpenGothic-side process
  heuristics; the original flee (`oCNpc::ThinkNextFleeAction`/`Fleeing` @ oNpc_Move.cpp)
  uses a projected flee point + `GetNearestWaypoint`, a structurally different algorithm with
  no single comparable constant. Not a crisp constant divergence.
- **Wake-on-approach / sleep-until-disturbed / herd / ZS_MM_Rtn wander** — driven by Daedalus
  states (ZS_MM_*, B_MM_*) and `PERC_DIST_MONSTER_*` script constants, not engine code.
  Out of engine-parity scope.

## DEFERRED candidate (low confidence — do NOT patch without verification)
**Sensing distance metric: center-to-center vs bbox-edge.**
- Original: `oCNpc::CanSense` @0x00740740 smell branch uses `GetDistanceToVobApprox`
  (bounding-box aware, edge-to-edge); `CanSee` uses vob-origin translations.
- OpenGothic: `canSenseNpc` (game/world/objects/npc.cpp:5033-5034) uses
  `qDistTo(oth.centerPosition())` (center-to-center, exact Euclidean; `qDistTo` at npc.cpp:731).
- Effect: for physically large monsters (troll/dragon/golem), edge-to-edge distance is smaller
  than center-to-center, so the original senses/flees them from marginally farther away than
  OpenGothic does. Real but minor; not specific to the panic subsystem and not a single
  surgical constant. DEFERRED: needs measured magnitude before any change is justified.

// NOTE: in original-game oCNpc::CanSense @0x00740740 and oCNpc::CanSee @0x00741c10.

## Excluded by brief (confirmed present, not re-reported)
- Aggro 360° vs FOV: `updateNearestEnemy` (npc.cpp:2305) passes `freeLos=true`, bypassing the
  FOV cone; original `CanSense` → `CanSee(...,0)` applies the ±91° (`0x5b`) cone. This is the
  already-known "enemy detection FOV" item.
