# Waynet: route edge cost uses Euclidean, original uses Manhattan

> DEFER: changing A* edge cost/heuristic to Manhattan alters which corridor wins globally; needs runtime path validation.

**Confidence:** Medium

## Original function + address

`zCWay::EstimateCost` (Gothic2.exe, 0x007aeaa0) computes the cost stored on each
waypoint connection (the `zCWay` "len" field at offset 4) as the **Manhattan
distance** between its two endpoint waypoints: `|dx| + |dy| + |dz|` (abs-sum of
the three componentwise differences, identical pattern to `GetNearestWaypoint`).
This stored value is exactly what `zCWayNet::AStar` (0x007b08d0) reads as the
per-edge g-cost when accumulating path length (`g(child) = g(node) + way.len`),
and the A* heuristic (`zCWayNet::EstimateCost`, 0x007b07d0) is likewise Manhattan
distance to the goal. So the entire original route-length accounting is Manhattan.

(Note: `zCWay::GetLength`, 0x007aeb10, is a *separate* octagonal-approximation
helper and is **not** the value AStar uses for cost — the cost field comes from
`zCWay::EstimateCost`.)

## OpenGothic location

`game/world/waypoint.cpp:64` `WayPoint::connect()` sets each connection length to
`int32_t(std::sqrt(qDistTo(w.pos)))`, i.e. the **Euclidean** distance. This `len`
is the per-edge weight summed in `WayMatrix::wayTo` (`waymatrix.cpp:341`,
`l1 = l0 + i.len`) when selecting the shortest route.

## Divergence

Manhattan edge weights over-cost diagonal connections relative to Euclidean by up
to a factor of ~sqrt(3) (in 3D) / sqrt(2) (in plane). When the waynet offers two
alternative routes between the same endpoints — one with longer but more
axis-aligned edges, one with shorter diagonal edges — the original's Manhattan
sum can favor the route OpenGothic's Euclidean sum rejects, and vice versa. This
changes which corridor an NPC actually walks. (The A*-vs-BFS search-structure
difference is intentional and not flagged; only the edge-weight metric is.)

## Proposed patch

```cpp
// FILE: game/world/waypoint.cpp  (WayPoint::connect, ~line 64)

// OLD
void WayPoint::connect(WayPoint &w) {
  int32_t l = int32_t(std::sqrt(qDistTo(w.pos)));
  if(l<=0)
    return;

// NEW
void WayPoint::connect(WayPoint &w) {
  // NOTE: in original-game the connection cost used by zCWayNet::AStar is
  // zCWay::EstimateCost (0x007aeaa0) = Manhattan distance |dx|+|dy|+|dz|,
  // not the Euclidean length.
  const Vec3 d = pos - w.pos;
  int32_t l = int32_t(std::abs(d.x) + std::abs(d.y) + std::abs(d.z));
  if(l<=0)
    return;
```
