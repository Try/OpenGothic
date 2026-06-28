# AI_LookAt / AI_PointAt resolve the target point with a SUBSTRING match where the original is EXACT-only

**Confidence:** Medium-High

## Original fn + address (prose)

`AI_LookAt` is consumed by `oCNpc::EV_LookAt` @0x00759a40 and `AI_PointAt` by
`oCNpc::EV_PointAt` @0x00759f40 (both in `oNpc.cpp`). Each resolves the script-supplied
target-name string to a position the **same** way and with **no substring step**:

1. `zCWayNet::GetWaypoint(name)` @0x007b0330 — a binary search over the way-net's sorted
   waypoint list using a full-length, **case-sensitive byte-for-byte EXACT** string compare
   (it never calls `zSTRING::Search`).
2. If that returns null, a single fallback `oCWorld::SearchVobByName(name)` (world vtable
   slot +0x4c, hash-table lookup at @0x00623fb0 / @0x00780610) — again a CRC32 + **full-length
   EXACT** name compare.

Neither path calls `oCNpc::FindSpot` @0x007400e0 and neither uses `zSTRING::Search`
(@0x0046c920, the case-folded substring routine). So for AI_LookAt/AI_PointAt the original
performs **exact name matching only** — there is no substring/"contains" behaviour, unlike the
free-point/spot externals (AI_GotoFP, Wld_IsFPAvailable) that go through `FindSpot`'s substring
search.

## OG file:line

- `game/game/gamescript.cpp:3303` — `GameScript::ai_lookat`: `auto to = world().findPoint(waypoint);`
- `game/game/gamescript.cpp:3626` — `GameScript::ai_pointat`: `auto to = world().findPoint(waypoint);`

`World::findPoint(name, inexact=true)` (default arg, `game/world/world.h:57`) runs the exact
start-point / `std::lower_bound` pass and then, on miss, a **substring** fallback loop over every
index point via `WayPoint::checkName(name, /*inexact*/true)` → `name.find(n)!=npos`
(`game/world/waymatrix.cpp:findPoint`, `game/world/waypoint.cpp:checkName`). Both call sites take
the default `inexact=true`, so OpenGothic adds a substring match the original never does.

## Divergence

Comparison-type mismatch: original AI_LookAt / AI_PointAt resolve the target by **exact** name
(waypoint-exact, then vob-name-exact). OpenGothic does exact-then-**SUBSTRING** over all
waypoints+free-points. When the exact lookup fails but some point's name *contains* the argument,
OpenGothic silently looks/points at that substring-matched point whereas the original would
resolve nothing (original would instead try an exact vob-name match). e.g. `AI_LookAt(self,"PATH")`
substring-hits an `OW_PATH_*` waypoint in OpenGothic but not in the original.

This is the substring-vs-exact axis only; the already-fixed FP-name upper-casing and the
`FindSpot` substring convention are unaffected (those externals are correct).

## Proposed patch

Pass `inexact=false` so the lookups stay exact-only, matching `GetWaypoint`/`SearchVobByName`.
The `findPoint(name,false)` overload already exists and is used by `ai_gotowp` (line 3430),
`npc.cpp:3594`, etc., so this is build-safe.

`game/game/gamescript.cpp:3301`-`3306` (`ai_lookat`):

OLD:
```cpp
void GameScript::ai_lookat(std::shared_ptr<zenkit::INpc> selfRef, std::string_view waypoint) {
  auto self = findNpc(selfRef);
  auto to  = world().findPoint(waypoint);
  if(self!=nullptr)
    self->aiPush(AiQueue::aiLookAt(to));
  }
```

NEW:
```cpp
void GameScript::ai_lookat(std::shared_ptr<zenkit::INpc> selfRef, std::string_view waypoint) {
  auto self = findNpc(selfRef);
  // NOTE: in original-game oCNpc::EV_LookAt @0x00759a40 the target name is resolved by
  // zCWayNet::GetWaypoint @0x007b0330 (EXACT, case-sensitive full-length compare) and, on miss,
  // by oCWorld::SearchVobByName (EXACT) -- it never calls FindSpot/zSTRING::Search, so there is no
  // substring match. findPoint() defaults to inexact=true (substring fallback); force exact.
  auto to  = world().findPoint(waypoint, false);
  if(self!=nullptr)
    self->aiPush(AiQueue::aiLookAt(to));
  }
```

`game/game/gamescript.cpp:3624`-`3629` (`ai_pointat`):

OLD:
```cpp
void GameScript::ai_pointat(std::shared_ptr<zenkit::INpc> npcRef, std::string_view waypoint) {
  auto npc = findNpc(npcRef);
  auto to  = world().findPoint(waypoint);
  if(npc!=nullptr && to!=nullptr)
    npc->aiPush(AiQueue::aiPointAt(*to));
  }
```

NEW:
```cpp
void GameScript::ai_pointat(std::shared_ptr<zenkit::INpc> npcRef, std::string_view waypoint) {
  auto npc = findNpc(npcRef);
  // NOTE: in original-game oCNpc::EV_PointAt @0x00759f40 the target name is resolved by
  // zCWayNet::GetWaypoint @0x007b0330 (EXACT) and, on miss, oCWorld::SearchVobByName (EXACT); it
  // never substring-matches. findPoint() defaults to inexact=true (substring); force exact.
  auto to  = world().findPoint(waypoint, false);
  if(npc!=nullptr && to!=nullptr)
    npc->aiPush(AiQueue::aiPointAt(*to));
  }
```
