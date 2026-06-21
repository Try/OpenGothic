# Music-zone selection ignores zone priority

**Confidence: High — APPLIED**

## Original
`oCZoneMusic::BuildTempZoneList` (Gothic2.exe `0x00641530`) collects every active
overlapping music zone and keeps the list **sorted by priority**, inserting each zone
ahead of the first existing node whose priority is `<= this` priority. The **head of
the list (highest `GetPriority` @`0x006410b0` value) is the winning zone**; equal
priorities tiebreak by camera ellipsoid weight (`GetCamPosWeightElps`, smaller = more
inside). The default zone (`oCZoneMusicDefault`) is appended last and used only when
the camera is inside no zone. Priority semantics: **higher value = higher priority**
(matches ZenKit's documented `VZoneMusic::priority`).

(An earlier analysis pass mis-read this as "smallest priority wins"; decompiling the
insertion loop shows the list is sorted **descending** and the head — highest priority
— is selected.)

## OpenGothic (before)
`game/world/worldsound.cpp` `tickSoundZone` took the **last** bbox-matching zone in
`std::vector` iteration order (with a sticky "keep current zone" shortcut), never
reading `priority`. `WorldSound::Zone` did not store it, even though ZenKit parses
`zenkit::VZoneMusic::priority`.

## Divergence
For overlapping zones (e.g. a city-district zone nested inside a region zone), the
played theme depended on VOB load order, not priority — so the wrong music could play.

## Fix (applied)
- Added `int32_t priority` to `WorldSound::Zone`, populated from `vob.priority` in
  `addZone`/`setDefaultZone`.
- `tickSoundZone` now selects the highest-`priority` containing zone (falling back to
  the default when inside none), replacing the last-match-wins loop.

Equal-priority tiebreak uses first-match (OpenGothic models zones as boxes, not
ellipsoids, so the original's camera-weight tiebreak has no exact counterpart).
Build-verified; needs an in-game check where music zones overlap.
