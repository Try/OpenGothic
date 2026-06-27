# Ambient FP-roam: free-point search uses 800-unit sphere instead of original 700-unit box

**Confidence:** Medium

## Original function + address

The script externals that drive ambient FP roaming all resolve to one helper,
`oCNpc::FindSpot` (Gothic2.exe @ `0x007400e0`, `oNpc.cpp`). The external glue in
`oGameExternal.cpp` calls it as follows:

- `Wld_IsFPAvailable(self,name)` -> `FindSpot(name, flag=1, radius=700.0)` (helper @ `0x006eb5b0`)
- `Wld_IsNextFPAvailable(self,name)` -> `FindSpot(name, flag=0, radius=700.0)` (helper @ `0x006eb860`)
- `Npc_IsOnFP(self,name)` -> `FindSpot(name, flag=1, radius=100.0)` (helper @ `0x006ebb10`)
- `AI_GotoFP` / `AI_GotoNextFP` -> `oCNpc::EV_GotoFP` @ `0x00685700` -> `FindSpot(name, isNext, radius=700.0)`

Inside `FindSpot`, the candidate set is gathered with
`zCBspBase::CollectVobsInBBox3D` over an **axis-aligned cube of half-extent
`radius` (=700) centered on the NPC's translation** (it builds `center +/- 700`
on each of X/Y/Z). The collected `zCVobSpot`s are then sorted by 3-D distance
(`zCVob::GetDistanceToVob2`) ascending and the **nearest** one that (a) substring-matches
the requested name, (b) passes `zCVobSpot::IsAvailable`, and (c) has
`oCNpc::FreeLineOfSight` is returned. So the effective reach is 700 along an
axis and up to ~1212 (700*sqrt(3)) toward a cube corner.

## OpenGothic file:line

`game/world/waymatrix.cpp:274-300` — `WayMatrix::findFreePoint(at, FpIndex, filter)`,
the routine that backs `World::findFreePoint` / `World::findNextFreePoint` and
therefore `Wld_IsFPAvailable`, `Wld_IsNextFPAvailable`, `AI_GotoNextFp`.

The search radius constant is `WayMatrix::distanceThreshold = 800.f`
(`game/world/waymatrix.h:52`), used as `float R = distanceThreshold;` at
`game/world/waymatrix.cpp:276`.

## Divergence

Two related mismatches in the ambient-roam free-point pick:

1. **Radius:** OpenGothic searches with `R = 800`, the original uses `700`. OG
   therefore reports/picks FPs that the original would not (e.g. a roam FP at
   ~750 units straight ahead: original = unavailable, OG = available), which
   shifts which `FP_ROAM*` an idling NPC is allowed to wander to and which it
   considers "available."
2. **Shape:** OpenGothic uses a **sphere** (`(w.pos-at).quadLength() < R*R`),
   the original uses an **axis-aligned box** (`|dx|,|dy|,|dz| <= 700`). Along a
   diagonal the original reaches farther than any sphere of radius 700/800,
   while along an axis a 700-cube is tighter than OG's 800-sphere. Neither the
   radius nor the shape matches.

`distanceThreshold` is shared with `WayMatrix::findNextPoint` (waypoint search,
line 85), so the fix is scoped to the free-point routine only to avoid
regressing ordinary waypoint selection.

## Proposed patch

Grep-verified symbols: `WayMatrix::findFreePoint` (waymatrix.cpp:274),
`WayPoint::pos` (`.x/.y/.z`, used at line 277/291), `WayPoint::isFreePoint()`
(waypoint.cpp:34), `Vec3::quadLength()` (used line 291). `<limits>` already
included (waymatrix.cpp:5).

OLD (`game/world/waymatrix.cpp:274-300`):
```cpp
const WayPoint *WayMatrix::findFreePoint(const Vec3& at, const FpIndex& ind,
                                         const std::function<bool(const WayPoint&)>& filter) const {
  float R = distanceThreshold;
  auto b = std::lower_bound(ind.index.begin(),ind.index.end(), at.x-R ,[](const WayPoint *a, float b){
    return a->pos.x < b;
    });
  auto e = std::upper_bound(ind.index.begin(),ind.index.end(), at.x+R ,[](float a,const WayPoint *b){
    return a < b->pos.x;
    });

  const WayPoint* ret=nullptr;
  auto  count = std::distance(b,e);(void) count;
  float dist  = R*R;
  for(auto i=b;i!=e;++i){
    auto& w  = **i;
    if(!w.isFreePoint())
      continue;
    float l = (w.pos - at).quadLength();
    if(l>=dist)
      continue;
    if(!filter(w))
      continue;
    ret  = &w;
    dist = l;
    }
  return ret;
  }
```

NEW:
```cpp
const WayPoint *WayMatrix::findFreePoint(const Vec3& at, const FpIndex& ind,
                                         const std::function<bool(const WayPoint&)>& filter) const {
  // NOTE: in original-game oCNpc::FindSpot @0x007400e0 candidate free points are gathered with
  // zCBspBase::CollectVobsInBBox3D over an axis-aligned cube of half-extent 700 around the npc
  // (Wld_IsFPAvailable/Wld_IsNextFPAvailable/AI_GotoFP all pass radius 700, oGameExternal.cpp
  // @0x006eb5b0/0x006eb860, EV_GotoFP @0x00685700) and the nearest name-matching/available one is
  // returned. OpenGothic used an 800-unit sphere; restore the 700-unit box.
  const float R = 700.f;
  auto b = std::lower_bound(ind.index.begin(),ind.index.end(), at.x-R ,[](const WayPoint *a, float b){
    return a->pos.x < b;
    });
  auto e = std::upper_bound(ind.index.begin(),ind.index.end(), at.x+R ,[](float a,const WayPoint *b){
    return a < b->pos.x;
    });

  const WayPoint* ret=nullptr;
  float dist  = std::numeric_limits<float>::max();
  for(auto i=b;i!=e;++i){
    auto& w  = **i;
    if(!w.isFreePoint())
      continue;
    const Vec3 d = w.pos - at;
    if(d.y<-R || d.y>R || d.z<-R || d.z>R) // X already bounded by [b,e); match original cube test
      continue;
    float l = d.quadLength();
    if(l>=dist)
      continue;
    if(!filter(w))
      continue;
    ret  = &w;
    dist = l;
    }
  return ret;
  }
```

This restores both the original 700-unit reach and the box (not sphere) shape
while preserving OG's nearest-first selection and the existing
locked/LOS/name filter, and leaves `distanceThreshold` (waypoint search)
untouched.
