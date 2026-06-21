# Music zone selection ignores zone priority

**Confidence:** High

## Original function + address

`oCZoneMusic::BuildTempZoneList` (0x00641530), called from
`oCZoneMusic::ProcessZoneList` (0x00640560).

The original collects every *active* music zone (camera inside the zone, and for
ellipsoid zones a camera-position weight `<= 1.0`) and inserts each into a list
sorted by the zone's priority field (the integer set by `SetPriority`, address
0x006410a0, read by `GetPriority`, 0x006410b0). The insertion comparison keeps the
list head equal to the zone with the **numerically smallest** priority value; when
two zones share the same priority, the tie is broken in favor of the zone with the
**larger** camera-position ellipsoid weight (`GetCamPosWeightElps`, 0x00641380).
`ProcessZoneList` then uses the list head (`*poVar5`) as the single winning zone and
derives the theme from its name. The default zone is only appended at the tail, so it
is used solely as a last resort.

So: overlapping music zones are resolved by priority, then by camera weight; the
default zone is the lowest-precedence fallback.

## OpenGothic file:line

`game/world/worldsound.cpp:224-238` (`WorldSound::tickSoundZone`).

The OG `WorldSound::Zone` struct (`worldsound.cpp:40-50`) stores only the bbox and
name; it never stores `zenkit::VZoneMusic::priority`. Selection is:

- keep `currentZone` if the player is still inside it, otherwise
- iterate `zones` and take the **last** zone whose bbox contains the player.

## Divergence

When music zones overlap (common in Gothic 2 — e.g. an inner city-district zone
nested inside a broader city zone), the original picks the highest-precedence zone by
priority. OpenGothic picks by `std::vector` iteration order (last bbox match wins),
which depends on world-load order and has no relation to the designer-assigned
priority. This selects the wrong music theme in overlapping-zone areas — a directly
audible, gameplay-different result.

(Note on sense: ZenKit documents `priority` as "higher = higher priority", but the
original engine's winning zone is the one with the *smallest* internal priority value.
The patch below matches the original engine's primary selection key.)

## Proposed patch

Store the priority on the zone and prefer the smallest-priority overlapping zone.

`game/world/worldsound.cpp` — Zone struct (around line 40):

OLD:
```cpp
struct WorldSound::Zone final {
  Tempest::Vec3 bbox[2]={};
  std::string   name;
```
NEW:
```cpp
struct WorldSound::Zone final {
  Tempest::Vec3 bbox[2]={};
  std::string   name;
  int32_t       priority = 0;
```

`game/world/worldsound.cpp` — addZone (around line 78):

OLD:
```cpp
  z.bbox[1] = {vob.bbox.max.x, vob.bbox.max.y, vob.bbox.max.z};
  z.name    = vob.vob_name;

  zones.emplace_back(std::move(z));
```
NEW:
```cpp
  z.bbox[1] = {vob.bbox.max.x, vob.bbox.max.y, vob.bbox.max.z};
  z.name    = vob.vob_name;
  z.priority = vob.priority;

  zones.emplace_back(std::move(z));
```

`game/world/worldsound.cpp` — tickSoundZone selection (lines 229-238):

OLD:
```cpp
  Zone* zone = def.get();
  if(currentZone!=nullptr && currentZone->checkPos(plPos)){
    zone = currentZone;
    } else {
    for(auto& z:zones) {
      if(z.checkPos(plPos)) {
        zone = &z;
        }
      }
    }
```
NEW:
```cpp
  // NOTE: in original-game oCZoneMusic::BuildTempZoneList selects the active
  // overlapping zone with the smallest priority value (default zone is the
  // last-resort fallback); ties are broken by camera-position weight.
  Zone* zone = def.get();
  if(currentZone!=nullptr && currentZone->checkPos(plPos)){
    zone = currentZone;
    } else {
    Zone* best = nullptr;
    for(auto& z:zones) {
      if(z.checkPos(plPos) && (best==nullptr || z.priority<best->priority))
        best = &z;
      }
    if(best!=nullptr)
      zone = best;
    }
```
