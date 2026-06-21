# Waynet: nearest-waypoint uses Euclidean, original uses Manhattan

> DEFER: changing the nearest-waypoint metric (Euclidean->Manhattan) alters NPC routing globally; Medium confidence and needs runtime path validation before applying.

**Confidence:** Medium

## Original function + address

`zCWayNet::GetNearestWaypoint` (Gothic2.exe, 0x007ad660). It iterates every
waypoint in the net and keeps the one minimizing the **Manhattan distance**
`|wp.x - p.x| + |wp.y - p.y| + |wp.z - p.z|` between the query position `p` and
the waypoint position (waypoint coords at offsets 0x44/0x48/0x4c, abs-sum of the
three componentwise differences). This is the routine used by every
`zCWayNet::FindRoute` overload (0x007b04d0, 0x007b0560, 0x007b05d0) to pick the
start waypoint of a route, and is the standard "nearest waypoint to a position"
selector throughout the engine.

## OpenGothic location

`game/world/waymatrix.cpp:62` `WayMatrix::findWayPoint(const Vec3& at, filter)`
selects the waypoint minimizing `(w.pos - at).quadLength()`, i.e. **Euclidean
squared distance** (`waymatrix.cpp:66`).

## Divergence

The two metrics rank candidate waypoints differently whenever the two nearest
candidates differ in how their offset is distributed across axes. Manhattan
penalizes axis-spread offsets more than Euclidean, so for a query point that sits
between a "diagonally near" waypoint and an "axis-aligned slightly farther"
waypoint, the original can choose the axis-aligned one while OpenGothic chooses
the diagonal one. Because the chosen waypoint is the entry node of an NPC's route
(`World::findWayPoint` is used by `Npc_GetNextWp`, AI routing start nodes, and
`findNextWayPoint`), a different nearest waypoint can yield a different walked
route and a different reported "next waypoint" to scripts.

## Proposed patch

```cpp
// FILE: game/world/waymatrix.cpp  (WayMatrix::findWayPoint, ~line 62)

// OLD
const WayPoint *WayMatrix::findWayPoint(const Vec3& at, const std::function<bool(const WayPoint&)>& filter) const {
  const WayPoint* ret =nullptr;
  float           dist=std::numeric_limits<float>::max();
  for(auto& w:wayPoints) {
    const float l = (w.pos - at).quadLength();
    if(l>=dist)
      continue;

// NEW
const WayPoint *WayMatrix::findWayPoint(const Vec3& at, const std::function<bool(const WayPoint&)>& filter) const {
  const WayPoint* ret =nullptr;
  float           dist=std::numeric_limits<float>::max();
  for(auto& w:wayPoints) {
    // NOTE: in original-game zCWayNet::GetNearestWaypoint (0x007ad660) ranks
    // waypoints by Manhattan distance |dx|+|dy|+|dz|, not Euclidean.
    const Vec3  d = w.pos - at;
    const float l = std::abs(d.x) + std::abs(d.y) + std::abs(d.z);
    if(l>=dist)
      continue;
```
