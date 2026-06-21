# External Npc_GetDistToWP returns Euclidean distance; original returns octagonal approximation

> DEFER: scripts are tuned to whatever metric the original uses, but the exact approximation is uncertain (this finding says octagonal/two-largest-abs; the waynet sweep reads Manhattan/sum-of-three) and changing a script-facing distance external has broad, hard-to-verify impact. Needs the exact metric confirmed + runtime.

Confidence: Medium-High

## Original function

`Npc_GetDistToWP` external handler at 0x006f2c30 (DefineExternals_Ulfi /
oGameExternal.cpp, G2 addon). It fetches the NPC and the waypoint via the
world waynet, then computes the difference of the NPC origin (matrix
translation) and the waypoint world position. It takes the absolute value of
each of the three component differences, sorts them, drops the smallest
component, and returns (in prose) the SUM OF THE TWO LARGEST absolute
component differences, truncated to int. It does NOT take a square root and
does NOT sum squares: it is the classic ZenGin octagonal/chamfer distance
approximation (`largest_abs + second_largest_abs`). When either the NPC or the
waypoint is missing it returns INT32_MAX.

Note this is distinct from `Npc_GetDistToNpc` (0x006f27a0), which calls
`zCVob::GetDistanceToVob` (0x0061b910) and IS a true `sqrt(dx^2+dy^2+dz^2)`.
So the two distance externals legitimately use different metrics in the
original; only the WP variant uses the approximation.

## OpenGothic

game/game/gamescript.cpp:2045 `GameScript::npc_getdisttowp` computes
`std::sqrt(npc->qDistTo(wp))`, i.e. true Euclidean distance
(`qDistTo` returns `quadLength()`, see game/world/objects/npc.cpp:721).

## Divergence

The approximation always OVERESTIMATES versus Euclidean, by up to ~41% on a
fully diagonal offset (e.g. offset (300,0,400): original returns 700, OG
returns 500; offset (a,a,a): original 2a, OG sqrt(3)*a ~= 1.73a). Scripts that
gate on `Npc_GetDistToWP(self,"WP") < D` therefore trip at a different real
radius in OG than in the retail engine — NPCs read as "closer" to waypoints in
OG. Secondary: original measures from the NPC origin (trafo translation);
OG measures from `centerPosition()`.

## Proposed patch

game/game/gamescript.cpp

OLD:
```cpp
  if(npc!=nullptr && wp!=nullptr){
    float ret = std::sqrt(npc->qDistTo(wp));
    if(ret<float(std::numeric_limits<int32_t>::max()))
      return int32_t(ret); else
      return std::numeric_limits<int32_t>::max();
    } else {
    return std::numeric_limits<int32_t>::max();
    }
```

NEW:
```cpp
  if(npc!=nullptr && wp!=nullptr){
    // NOTE: in original-game Npc_GetDistToWP (oGameExternal.cpp) does NOT use
    // sqrt(dx^2+dy^2+dz^2). It returns the octagonal approximation: the sum of
    // the two largest absolute component differences (smallest axis dropped).
    auto  d  = npc->position() - wp->position();
    float ax = std::abs(d.x), ay = std::abs(d.y), az = std::abs(d.z);
    if(ax<ay) std::swap(ax,ay);
    if(ax<az) std::swap(ax,az);
    if(ay<az) std::swap(ay,az);
    float ret = ax + ay; // two largest
    if(ret<float(std::numeric_limits<int32_t>::max()))
      return int32_t(ret); else
      return std::numeric_limits<int32_t>::max();
    } else {
    return std::numeric_limits<int32_t>::max();
    }
```

(If `npc->position()` is the feet origin and `wp->position()` the waypoint
world pos, this also restores the original origin-vs-center detail. Verify the
WayPoint accessor name; `findWayPoint` already yields the `wp` used above.)
