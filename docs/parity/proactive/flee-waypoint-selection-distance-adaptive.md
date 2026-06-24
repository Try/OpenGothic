# Flee waypoint selection: distance-adaptive away-trace vs. fixed 5 m radius search

**Confidence:** Low–Medium (real divergence, but corrective fix is a large rewrite = feel-tuning → DEFERRED)

## Scope note (where the flee TRIGGER actually lives)
The prompt's primary target — the flee-threshold HP percentage that triggers
`ZS_MM_Flee` / `B_AssessFlee`, the morale check, the coward/brave flag, and the
per-NPC flee HP — is **not** in the engine. In the original `Gothic2.exe` these
are computed entirely in Daedalus (`B_AssessFlee` / `ZS_MM_Flee`, reading `aivar`
and HP percentages), and OpenGothic runs those scripts unmodified. Confirming this:
- ZenKit's `INpc` (`lib/ZenKit/include/zenkit/addon/daedalus.hh`) has **no** `flee`
  field; only `protection[]` and `fight_range_*` exist (lines 40–46, 193).
- Neither OpenGothic's `game/game/fightalgo.cpp` nor the original engine flee
  functions contain any HP-percentage comparison driving an auto-flee.
So there is **no engine-side flee-trigger-threshold divergence to fix.** The only
engine-side flee logic is the *movement / waypoint selection* once the script has
issued `AI_Flee`.

## Original function + address (prose only)
- `oCNpc::Fleeing` @ `0x006820c0` and its twin `oCNpc::ThinkNextFleeAction`
  @ `0x006820d0` (identical body) implement the per-tick flee movement step.
  (`oCNpc::AI_Flee` @ `0x00683210` is an empty stub.)
- Behavior: the enemy NPC pointer lives at `this+0x498` (verified via
  `oCNpc::SetEnemy` @ `0x00734bc0`, which reads/writes `this+0x498`). `Fleeing`
  first requires `FreeLineOfSight(enemy)` — with no line of sight to the enemy it
  returns immediately and issues no new movement. It then computes the vector from
  the enemy to self and scales it by **-2.0f** (constant `0xC0000000`), i.e. a
  point in the *away* direction at roughly **twice the current enemy distance**.
  It probes up to 5 stepped candidate points outward, snapping each to the way-net
  via `zCWayNet::GetNearestWaypoint` / `GetSecNearestWaypoint`, halving/scaling the
  offset (`1/n`, and a `-1.0f` flip via `0xBF800000`) when a candidate fails or
  repeats the last waypoint (`this+0x4c0`). On success it sends
  `oCMsgMovement(3, waypointName)` to walk there; otherwise it falls back to a
  RobustTrace toward the away-point at range `10000.0f` (`this+0x4e4`).

So the original flee target distance is **adaptive to how far the enemy is** and is
resolved against the global way-net, in the direction directly away from the enemy.

## OpenGothic file:line
`game/world/objects/npc.cpp:1976` — `Npc::implAiFlee` (dispatched from the
`AI_Flee` case at `game/world/objects/npc.cpp:2766`).

## Divergence
`implAiFlee` uses a **fixed 5 m radius** search (`const float maxDist = 5*100;`,
line 1986) via `owner.findWayPoint`, selecting the candidate that **maximizes
distance from the enemy** (`oth.qDistTo(&p) > oth.qDistTo(wp)`), and additionally
**filters out** waypoints with `useCounter()>0`, `underWater`, and any not reachable
by `canRayHitPoint(...)`. If no waypoint qualifies (or the best one is no farther
from the enemy than the NPC already is) it simply turns/runs directly away.

Differences vs. original:
1. **Search range:** OG is a constant 5 m; the original probes outward to ~2× the
   enemy distance, so OG NPCs effectively flee shorter and re-evaluate more often.
2. **Selection metric:** OG maximizes farthest-from-enemy within the radius; the
   original takes the way-net point nearest the away-trace point.
3. **Filters:** OG rejects occupied (`useCounter>0`) and underwater waypoints and
   requires a clear ray to the waypoint; the original applies none of these.
4. **Precondition:** the original gates the whole step on `FreeLineOfSight(enemy)`;
   OG has no such enemy-LOS gate (it rays to the *waypoint* instead).

## Proposed patch
**DEFERRED.**

Reason: This is not a single-line threshold/sign bug — it is a complete
reimplementation of the flee-movement search with different range model
(adaptive vs. fixed), different selection metric, different way-net API
(`GetNearestWaypoint`/`GetSecNearestWaypoint` have no clean OpenGothic equivalent
exposed; OG uses `World::findWayPoint`), and different/absent candidate filters.
Replacing OG's algorithm wholesale would change emergent NPC pathing "feel" with
no objective oracle, and the affected fields (`0x4a8`/`0x4b4` away-vectors,
`0x4c0` last-waypoint, `0x4e4` robust-trace range) have no 1:1 OpenGothic
counterparts to verify against. Per the hard rules ("Feel-tuning → DEFERRED";
"Empty beats false positives"), no surgical, build-verifiable, high-confidence
fix exists here.

The genuinely actionable flee-trigger logic the prompt asks about lives in
Daedalus scripts that OpenGothic already executes unmodified, so there is no
engine divergence to patch for the trigger threshold itself.
