# Map marker: original falls back to the world bounding box when level-coords are unset; OpenGothic divides by an empty rect

**Confidence:** Medium. The code divergence is High-confidence (verified directly in
`oCViewDocumentMap::UpdatePosition`), but its *behavioral observability in stock
Gothic II content is Low*, because the shipped `Doc_MapCoordinates` wrapper always
sets both the level name and the level-coords together. Distinct from the
already-documented orientation divergence (`map-compass-marker-8dir-arrows.md`) and
from the already-fixed `Doc_SetMargins` apply-flag.

## Original function + address (prose only)

`oCViewDocumentMap::UpdatePosition` at **0x0068d7b0** (helpers
`oCViewDocumentMap::SetLevelCoords` at 0x0068d770, written in argument order
left/top/right/bottom into object offsets 0x210/0x214/0x218/0x21c).

What it does, in prose, for the world-rect used to map player position to pixels:

1. It tests the four stored level-coords (0x210/0x214/0x218/0x21c). The test
   succeeds — i.e. the script-supplied coords are used — when **at least one** of the
   four is non-zero.
2. If, and only if, **all four are zero** (the un-initialized state — the ctor at
   0x0068d340 zeroes 0x210..0x21c), it instead reads the active world's bounding box
   (the min/max `zVEC3` pair fetched off the world's BSP/bbox at `world+0x1b4`,
   elements `+4` and `+0x10`) and uses *that* min-X/max-X and min-Z/max-Z as the
   left/right and top/bottom world rectangle.
3. Either way it then builds the pixel-rect-to-world-rect linear scale and converts
   the player translation to a pixel position.

So the original guarantees a sane, finite world rectangle for the marker even when a
map document set a level but never set explicit level-coords: it silently substitutes
the full world extents.

The coordinate-to-pixel math itself was cross-checked instruction-by-instruction and
is faithful in OpenGothic. Original (after picking the world rect):
`screenX = left_px + (worldX - L)*W_px/(R - L)` and
`screenY = top_px + H_px*(T - worldZ)/(T - B)`, which is algebraically identical to
OpenGothic's `wx = (pos.x - L)/(R - L)`, `wy = (pos.z - T)/(B - T)`. No sign / axis /
Y-flip divergence exists in the transform. The divergence is strictly the *missing
fallback* that supplies (L,T,R,B) when the script left the coords at zero.

## OpenGothic file:line

`game/ui/documentmenu.cpp:100-107` (marker position) and
`game/gothic.cpp:1203-1208` (`Gothic::doc_setlevelcoords`). The bounds field and its
default:

```
game/ui/documentmenu.h:36      Tempest::Rect     wbounds;     // default-constructed => Rect(0,0,0,0)
game/gothic.cpp:1207           doc->wbounds = Rect(left,top,right-left,bottom-top);
game/ui/documentmenu.cpp:102   float wx = (pos.x-float(document.wbounds.x))/float(document.wbounds.w);
game/ui/documentmenu.cpp:103   float wy = (pos.z-float(document.wbounds.y))/float(document.wbounds.h);
```

`DocumentMenu::Show::wbounds` is default-constructed to `Rect(0,0,0,0)` (see the
`// TODO: set default values for these` comment in `documentmenu.h:28`). If a document
has `showPlayer==true` (level matched) but `Doc_SetLevelCoords` was never called,
`wbounds.w == 0` and `wbounds.h == 0`, so lines 102-103 divide by zero, producing
NaN/inf marker pixel coordinates (marker invisible or pinned to a corner) instead of
the original's full-world-extent fallback. There is no world-bounding-box fallback
anywhere in OpenGothic's document/map path (grep-verified: `wbounds` is referenced
only at `documentmenu.h:36`, `documentmenu.cpp:102-103`, and `gothic.cpp:1207`).

## Divergence

When a map document sets its level (so the marker is enabled) but does not supply
level-coords, the original positions the marker using the world's bounding box,
whereas OpenGothic divides by a zero-sized rect and yields a garbage marker position.
In stock Gothic II this path is not exercised — the engine-side `Doc_MapCoordinates`
Daedalus wrapper always issues `Doc_SetLevel` and `Doc_SetLevelCoords` as a pair — so
the visible impact is limited to mods / documents that call `Doc_SetLevel` (or
`Doc_CreateMap`+level match) without coords.

## Proposed patch

**DEFERRED.** Reasons:

1. Low observability: no shipped Gothic II content reaches the all-zero-coords path,
   so a fix is not build-verifiable against original behavior through normal play —
   "empty beats false positives."
2. A faithful fix must source the *world bounding box* (min/max X and Z of the loaded
   level) and substitute it when `wbounds` is empty. OpenGothic does expose world
   bounds, but wiring the correct min/max-X/Z into `DocumentMenu::Show` at
   `doc_show`/paint time (and matching the original's exact bbox field, world+0x1b4
   element +4/+0x10) needs the OG `World`/`DynamicWorld` bounds accessor pinned down
   first; proposing it blind risks a wrong-rect regression.

If pursued, the surgical change is: in `DocumentMenu::paintEvent`, before the
divisions at lines 102-103, guard `document.wbounds.w==0 || document.wbounds.h==0`
and substitute the current world's XZ bounding box (or skip drawing). This is left
DEFERRED pending the world-bounds accessor and confirmation the path is worth
emulating.

// NOTE: in original-game oCViewDocumentMap::UpdatePosition @0x0068d7b0 the player
// marker uses the script level-coords (offsets 0x210/0x214/0x218/0x21c) only when at
// least one is non-zero; when all four are zero it falls back to the active world's
// bounding box (world+0x1b4, vec +4/+0x10) instead of dividing by an empty rect.
