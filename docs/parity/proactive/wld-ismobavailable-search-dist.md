# Wld_IsMobAvailable / Wld_GetMobState: mob search radius 1000 vs original ~500

**Confidence: Medium**

## Original function

`Wld_IsMobAvailable` (Gothic2.exe, oGameExternal.cpp handler `0x6f5e20`) and
`Wld_GetMobState` (handler `0x6ed880`) both locate the mob through
`oCNpc::FindMobInter(name)` (`oNpc.cpp`).

`FindMobInter` builds a bounding box of the NPC's translation **plus/minus 500.0**
(the float constant `0x43fa0000` = 500.0) on every axis, then `CollectVobsInBBox3D`
gathers candidate vobs inside that cube. Of the candidates whose object name contains
the requested scheme and that pass a `FreeLineOfSight` test, it returns the nearest.
The effective search reach is therefore a cube of half-extent **500** units (with a
line-of-sight requirement).

## OpenGothic

`WorldObjects::availableMob` — `game/world/worldobjects.cpp:859`:

```cpp
const float  dist = MOBSI_SEARCH_DISTANCE;   // = 100*10 = 1000
...
interactiveObj.find(pl.position(),dist,[&](Interactive& i){
  if(i.isAvailable() && i.checkMobName(dest)) {
    float d = pl.qDistTo(i);
    if(d<curDist){ ... }
```

`MOBSI_SEARCH_DISTANCE = 100*10` (`game/game/constants.h:130`) is used here as a
**spherical radius of 1000** (`curDist = dist*dist`), with no line-of-sight test.

## Divergence

The OpenGothic search radius (1000, sphere) is roughly twice the original's effective
reach (500, cube). An NPC routine calling `Wld_IsMobAvailable` / `Wld_GetMobState` will
report a mob as available (and non-`-1` state) from up to ~1000 units away, where the
original reports it unavailable beyond ~500. This changes routine/AI mob-selection: NPCs
pick up and walk to mobsis from farther than vanilla.

The constant is shared with a vertical physics ray in `interactive.cpp:947`, so retuning
the global enum is unsafe; the surgical fix is a local literal in `availableMob`.

## Proposed patch

File: `game/world/worldobjects.cpp`

OLD:
```cpp
Interactive *WorldObjects::availableMob(const Npc &pl, std::string_view dest) {
  const float  dist = MOBSI_SEARCH_DISTANCE;
```

NEW:
```cpp
Interactive *WorldObjects::availableMob(const Npc &pl, std::string_view dest) {
  // NOTE: in original-game oCNpc::FindMobInter (used by Wld_IsMobAvailable / Wld_GetMobState)
  // the candidate mobs are collected from a bounding box of npc-pos +/- 500.0 (cube
  // half-extent 500), not a 1000-unit radius.
  const float  dist = 500.f;
```
