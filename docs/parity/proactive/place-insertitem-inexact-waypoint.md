# Wld_InsertItem resolves the spawnpoint with substring (inexact) name matching

**Confidence:** Medium (the divergence is verified on both sides; gameplay impact is
limited to non-exact spawnpoint names, so it is real but edge-case)

## Original function + address (prose)

The script external `Wld_InsertItem(item, "SPAWNPOINT")` is handled by
`FUN_006e0520` (the `Wld_InsertItem` external in
`P:\dev\g2addon\release\Gothic\_ulf\oGameExternal.cpp`). After creating the item
vob via the object factory, it resolves the spawnpoint to a world position in two
steps, both of which use **exact** name lookups:

1. `zCWayNet::GetWaypoint(name)` (`0x007b0330`). This is a binary search over the
   waynet's name-sorted waypoint list doing exact, case-insensitive name
   comparison. There is no substring / "contains" fallback.
2. Only if that returns null does it fall back to `zCWorld::SearchVobByName(name)`
   (`0x00780610`), which likewise matches a vob by its exact object name.

The resolved position is applied with `zCVob::SetPositionWorld` (`0x0061bb70`),
which writes only the translation column and leaves rotation at identity (so the
inserted item keeps identity orientation — confirmed; this is why the separate
`itemplace-waypoint-direction` heading change was rightly deferred). If neither
lookup matches, the engine logs `"Wld_InsertItem(): ... position vob not found"`
and the item ends up at the origin. At no point does the original perform a
partial/substring name match.

## OpenGothic file:line

`game/world/worldobjects.cpp:652`

```cpp
const WayPoint* waypoint = owner.findPoint(at);
```

`World::findPoint` (`game/world/world.h:57`) defaults its second parameter to
`inexact=true`, and `WayMatrix::findPoint` (`game/world/waymatrix.cpp:142-160`)
implements that flag as a substring fallback: after the exact start-point and
`indexPoints` lookups fail, it loops every indexed point and returns the first one
whose `checkName(name)` succeeds, where `WayPoint::checkName`
(`game/world/waypoint.cpp:42-50`) returns true when `name.find(n)!=npos`
(`inexact` branch).

## Divergence

For `Wld_InsertItem`, OpenGothic accepts a spawnpoint string that is merely a
**substring** of a real waypoint/freepoint name and places the item there, whereas
the original only accepts an exact name (then an exact vob name) and otherwise
fails the placement (item dropped at origin + error log). Example: a script /
mod calling `Wld_InsertItem(ItMiSword, "OW_PATH_036")` when only `OW_PATH_036_01`
exists places the sword on that path in OpenGothic but is a "position vob not
found" no-placement in the original. The exact-match cases (the overwhelming
majority of vanilla calls) behave identically; only non-exact names diverge.

## Proposed patch

`game/world/worldobjects.cpp:652`

```cpp
// OLD
  const WayPoint* waypoint = owner.findPoint(at);
// NEW
  // NOTE: in original-game the Wld_InsertItem external (FUN_006e0520,
  // oGameExternal.cpp) resolves the spawnpoint via zCWayNet::GetWaypoint
  // @0x007b0330 (exact, binary-search name match) with a zCWorld::SearchVobByName
  // @0x00780610 (exact vob name) fallback -- there is no substring match. Request
  // an exact lookup so a partial name does not place the item at the wrong point.
  const WayPoint* waypoint = owner.findPoint(at,false);
```

`findPoint(at,false)` still resolves exact waypoints, start-points and freepoints
(`waymatrix.cpp:145-154`); it only suppresses the substring fallback
(`waymatrix.cpp:155-160`), matching the original's exact-only behaviour. Surgical,
build-verifiable, no signature change.

### Secondary observations (DEFERRED)

- The original's second-stage fallback `zCWorld::SearchVobByName` matches *any*
  named world vob, not just waynet points; OpenGothic's `findPoint` only searches
  the waynet (way/free/start points). Inserting an item at the name of a plain
  named decoration vob would succeed in the original and fail in OpenGothic. No
  surgical fix exists (OpenGothic has no general name->vob index here), so this is
  deferred.
- The original creates the item vob even for an empty spawnpoint or an unresolved
  name (placing it at the origin with an error), whereas `GameScript::wld_insertitem`
  (`game/game/gamescript.cpp:2124`) early-returns on `spawnpoint.empty()` and
  never creates the vob. This only matters for malformed script calls and is
  deferred as low-impact.
