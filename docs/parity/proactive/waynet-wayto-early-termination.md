# Waynet route search settles `begin` waypoints before their distance is final

**Confidence:** Medium (genuine logic divergence; fix is NOT surgical → DEFERRED)

## Original function + address

The original route search is `zCWayNet::AStar` (private, `zCWayNet::AStar @0x007b0...`,
called from `zCWayNet::FindRoute @0x007b0xxx`, `P:\dev\g2addon\release\ZenGin\_ulf\zWaynet.cpp`).

It is a textbook A* over the waypoint graph: an open list (`zCListSort<zCWaypoint>`
kept sorted by `f = g + h`) and a per-search generation stamp (`this+0x48`). Each
iteration pops the *lowest-f* open node, marks it closed, and stops only when the
popped node equals the goal. A node is re-opened when a strictly cheaper `g` is
discovered (`if(*(pzVar13+0x28) <= iVar10)` guard plus the open/closed re-insert
logic). Because A* never commits a node's `g`-value until that node is *popped* as
the cheapest open node, the distance assigned to the start/goal waypoint is always
the true shortest-path cost at the moment it is used.

## OpenGothic file:line

`game/world/waymatrix.cpp:327-336` (`WayMatrix::wayTo`, the multi-source BFS run
*from* `end`):

```cpp
while(front->size()>0) {
  bool done = true;
  for(size_t i=0; i<beginSz; ++i)
    if(begin[i]->pathGen!=pathGen) {
      done = false;
      break;
      }
  if(done)
    break;                    // <-- stops as soon as every begin is *reached*
  ...
  }
```

The subsequent start-selection (`game/world/waymatrix.cpp:354-360`) and the route
reconstruction (`game/world/waymatrix.cpp:365-379`) both consume
`begin[i]->pathLen` as if it were the final shortest distance.

## Divergence

OpenGothic replaces the original A* with a relaxation-style BFS that expands the
graph **layer by layer outward from `end`** and terminates the moment *every*
`begin[i]` has merely been *touched* (`pathGen==pathGen`), not when its distance is
*settled*. Edge weights are non-uniform (`Conn::len` = Euclidean length), so the
first time a waypoint is reached is generally **not** along its shortest path.

Concrete failure: suppose `end → A` has edge cost 100 and `A → begin1` cost 1,
while `end → B` (cost 10) `→ C` (cost 10) `→ begin1` (cost 1) gives a far cheaper
total. The layer that first reaches `begin1` is the one that processed `A`
(reaching `begin1` at ~101); in that same layer `C` is only just discovered, so the
cheaper `…→C→begin1` (~21) has not yet been relaxed. The `done` test trips and the
loop `break`s, leaving `begin1->pathLen ≈ 101`. The reconstruction then either
returns a needlessly long route or — because the greedy descent in lines 365-379
requires each hop to strictly decrease `pathLen` along an edge that satisfies
`pathLen+len <= l0` — can fail to find a descending neighbour and return an empty
`WayPath` (line 376), i.e. a spurious "no route".

The relaxation at line 343 (`if(w.pathGen!=pathGen || w.pathLen>l1)`) *does*
re-push improved nodes, so the algorithm would eventually converge (SPFA-style) if
allowed to run to `front->size()==0`; the early `break` is what cuts it off before
convergence. The original A* has no analogous premature stop — it always settles
the relevant node optimally before using its cost.

## Proposed patch

**DEFERRED.**

Reason: the only correct fix is to let the search converge before reading
`begin[i]->pathLen` — e.g. remove the `done`/`break` early-out so the loop runs
until `front` empties (full relaxation), or re-architect `wayTo` as a proper
priority-queue Dijkstra/A* matching the original. Both are **algorithm-level**
changes, not a localized one-liner: removing the early-out alters the asymptotic
cost of every route query (the `break` is the routine's performance guard) and
changes which concrete route is produced for a large class of inputs, so it cannot
be verified as "parity" by inspection or a narrow build check. It is also entangled
with the already-DEFERRED `waynet-edge-cost-metric` finding: the *route chosen*
depends jointly on (a) this termination bug and (b) the Manhattan-vs-Euclidean /
`int(sqrt)` edge cost, so the two must be resolved together rather than patched in
isolation. Deferring until the edge-cost metric is settled and a faithful
A*/Dijkstra port can be designed and play-tested.

Verified OG symbols: `WayMatrix::wayTo` (`game/world/waymatrix.h:36`),
`WayPoint::pathGen`/`pathLen` (`game/world/waypoint.h:44-45`),
`WayPoint::Conn::len` (`game/world/waypoint.h:40`),
`WayPoint::connections()` (`game/world/waypoint.h:50`).
