# Free-point name match: empty fpName must match ANY free point (match-all), OpenGothic matches nothing

**Confidence:** Medium-High

## Original function + address (prose only)

- `oCNpc::FindSpot` at `0x007400e0` is the single selection core for free points. It is
  called by the script externals `Wld_IsFPAvailable` (`FUN_006eb5b0`, passing flag `1`) and
  `Wld_IsNextFPAvailable` (`FUN_006eb860`, passing flag `0`), and by the native movement
  message handler `oCNpc::EV_GotoFP` at `0x00685700` (which derives the flag from the
  message). All three pass the uppercased free-point name and a search radius of `700.0`.
- Inside `FindSpot`, the per-candidate name test is `spotName.Search(0, fpName, 1) >= 0`,
  i.e. `zSTRING::Search` at `0x0046c920`. Tracing that routine for an **empty pattern**
  (`fpName == ""`): the pattern pointer is non-null and the count argument is `1`, so the
  early `-1` guard is skipped; the computed pattern length is `0`, the outer test
  `(len != 0) || (strLen < startPos)` is false, so the running index keeps its initial value
  `0` (it never becomes `0xFFFFFFFF`), and the function returns `0`. A return of `0` means
  "found at position 0", so `>= 0` is true. **Conclusion: in the original, an empty fpName
  matches every free-point/spot name**, and `FindSpot` therefore returns the nearest
  available free point of ANY name within the radius.

## OpenGothic file:line

- `game/world/waypoint.cpp:42-50` — `WayPoint::checkName`: `if(n.empty()) return false;`
- `game/world/waymatrix.cpp:255-258` — `WayMatrix::findFpIndex` builds the candidate index
  with `if(!w.checkName(name)) continue;`
- Concrete in-engine caller broken by this: `game/world/worldobjects.cpp:315-321`
  `WorldObjects::addNpc(npcInstance, pos)` calls `owner.findFreePoint(pos, "")` as the
  fallback when no way-point is near, with the self-documenting comment
  `// vanilla assign some point to all npc's`. Also reachable from script via
  `GameScript::wld_isfpavailable` / `wld_isnextfpavailable` (`game/game/gamescript.cpp:1761-1776`).

## Divergence

For an empty name, `WayPoint::checkName("")` returns `false`, so `findFpIndex("")` produces an
empty candidate set and every free-point lookup returns `nullptr`. The original instead treats
empty fpName as a wildcard that matches all free points and returns the nearest available one.

Practical effects:
- `Wld_IsFPAvailable(self, "")` / `Wld_IsNextFPAvailable(self, "")` return `false` in
  OpenGothic but `true` (when any usable FP is within 700u) in the original.
- The internal NPC-placement fallback at `worldobjects.cpp:318` never finds a free point — the
  NPC is left with an empty way-point name — defeating the documented "assign some point to all
  npc's" intent that mirrors the original's any-name nearest-FP behavior.

Non-empty names are NOT affected: the original's `Search(spotName, fpName)` is a case-insensitive
substring test (match anywhere), which equals OpenGothic's `checkName` inexact path
(`name.find(n) != npos`). Only the empty-name (match-all) case diverges.

## Proposed patch

Scope the fix to the free-point index builder so exact named-waypoint matching
(`checkName(name,false)` callers, mob checks) is untouched.

`game/world/waymatrix.cpp` (in `WayMatrix::findFpIndex`):

OLD:
```cpp
  for(auto& w:freePoints){
    if(!w.checkName(name))
      continue;
    id.index.push_back(&w);
    }
```
NEW:
```cpp
  for(auto& w:freePoints){
    // NOTE: in original-game oCNpc::FindSpot @0x007400e0 the name test is
    // zSTRING::Search(spotName, fpName) @0x0046c920, which returns 0 (match) for an
    // empty pattern, so an empty fpName matches ANY free point. checkName() returns
    // false on empty, so treat empty name as match-all here to preserve that behaviour.
    if(!name.empty() && !w.checkName(name))
      continue;
    id.index.push_back(&w);
    }
```

This makes `findFreePoint(pos, "")` / `Wld_IsFPAvailable(self, "")` consider all free points
(nearest-available selection still applied downstream in `findFreePoint`/the World filters),
matching `FindSpot`'s wildcard semantics, while leaving every non-empty lookup identical.

Grep-verified symbols: `WayMatrix::findFpIndex` (`waymatrix.cpp:245`), `freePoints` member,
`WayPoint::checkName` (`waypoint.cpp:42`, default `inexact=true`).
