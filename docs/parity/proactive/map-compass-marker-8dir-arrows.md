# Map player-marker: original uses 8 discrete directional arrow textures, OpenGothic rotates one texture continuously

**Confidence:** High (texture-selection logic), behavioral. Visual-fidelity divergence, not a crash.

## Original function + address (prose only)

`oCViewDocumentMap::UpdatePosition` at **0x0068d7b0** (with helper
`oCViewDocumentMap::SetLevelCoords` at 0x0068d770) is the per-frame routine that
positions and orients the player marker on an open world-map document.

What it does, in prose:

1. It dynamic-casts the active world, fetches the world filename, and compares it
   against the level name configured for this map document (the field written by
   `SetLevelCoords`/`SetLevel`). If they differ it closes the marker FX and returns,
   so the marker is only drawn when the document's level matches the current world.
2. It reads the four level-coords stored by `SetLevelCoords` — at object offsets
   0x210/0x214/0x218/0x21c, written in argument order (left, top, right, bottom) —
   builds a pixel-rect-to-world-rect linear scale, and converts the player's world
   translation (from `zMAT4::GetTranslation` on the player vob) into a pixel
   position, then centers it by subtracting half the marker dimension. This is the
   coordinate transform.
3. **For the marker orientation it calls `oCNpc::GetAngles` to get the player's
   heading (yaw), then quantizes that heading into one of EIGHT 45-degree sectors.**
   The sector boundaries are the float constants 22.5, 67.5, 112.5, 157.5 and their
   negatives (verified: 0x41b40000=22.5, 0x42870000=67.5, 0x42e10000=112.5,
   0x431d8000=157.5). Heading near 0 maps to the "up" sector.
4. Per sector it selects one of eight base texture-name fragments — `O`, `RO`, `R`,
   `RU`, `U`, `LU`, `L`, `LO` (German Ost/Rechts/Unten/Links compass abbreviations)
   — concatenates `.TGA`, and calls `oCViewDocument::SetTexture` followed by
   `SetPixelPosition`. The strings `O.TGA` and `U.TGA` are present in the binary at
   0x008b6d30 / 0x008b6d38, and the directional fragments are concatenated with the
   `.TGA` suffix at runtime in this function. So the original swaps among **eight
   pre-rotated arrow bitmaps**; it never rotates a single texture.

## OpenGothic file:line

`game/ui/documentmenu.cpp:14` and `game/ui/documentmenu.cpp:100-114`
(marker setup + per-frame draw). Bounds wiring in
`game/gothic.cpp:1201-1206` (`Gothic::doc_setlevelcoords`).

OpenGothic loads a single cursor texture once:

```
game/ui/documentmenu.cpp:14   cursor = Resources::loadTexture("U.TGA");
```

and in `paintEvent` draws that one texture rotated by a continuous angle:

```
game/ui/documentmenu.cpp:109   p.pushState();
game/ui/documentmenu.cpp:110   p.translate(cx,cy);
game/ui/documentmenu.cpp:111   p.rotate(-pl->rotation()-90);
game/ui/documentmenu.cpp:112   p.drawRect(-cursor->w()/2,-cursor->h()/2, cursor->w(),cursor->h());
game/ui/documentmenu.cpp:113   p.popState();
```

## Divergence

The original renders the on-map player marker as one of eight discrete directional
arrow textures (`O/RO/R/RU/U/LU/L/LO` + `.TGA`), chosen by snapping the player's
heading to the nearest 45-degree compass sector. OpenGothic instead loads only the
"up" arrow `U.TGA` and rotates it by a continuous angle (`-rotation()-90`). The
result is a marker that rotates smoothly through all 360 degrees rather than the
original's 8 stepped orientations, and that depends on whatever single `U.TGA`
asset exists rather than the eight per-direction assets the original game ships and
expects. The coordinate-to-pixel transform itself (lines 100-107) is structurally
faithful to the original's scale-and-offset mapping; only the marker-orientation
logic diverges.

## Proposed patch

**DEFERRED.** Reasons:

1. The faithful behavior requires selecting among eight textures
   (`O.TGA`,`RO.TGA`,`R.TGA`,`RU.TGA`,`U.TGA`,`LU.TGA`,`L.TGA`,`LO.TGA`) by sector,
   which is an asset-availability question: it must be verified that the Gothic II
   asset packages actually contain all eight TGAs (the original concatenates the
   names at runtime, implying they exist, but OpenGothic only ever references
   `U.TGA` and `O.TGA` today — see `game/ui/gamemenu.cpp:303`). Proposing the
   8-texture swap without confirming the assets risk a regression to a missing-
   texture / blank marker, violating "empty beats false positives."

2. The continuous-rotation approach is arguably a deliberate visual improvement and
   is not a correctness bug (marker still points the right way). The only firm
   parity claim is the discretization + multi-asset selection; mapping the original
   sector-to-texture table and the exact yaw sign/offset onto Tempest's `rotate`
   sign convention (note OG already applies `-90`) needs the `oCNpc::GetAngles` yaw
   convention pinned down before a build-verifiable edit can be written.

If pursued, the surgical change would be: in `DocumentMenu` keep eight texture
pointers, and in `paintEvent` replace the `rotate`/single-`cursor` block with a
sector lookup `idx = round(heading/45) mod 8` indexing the eight textures, drawing
unrotated and centered. This is left DEFERRED pending asset verification and yaw-
convention confirmation.

// NOTE: in original-game oCViewDocumentMap::UpdatePosition @0x0068d7b0 the map
// player marker is one of 8 directional arrow bitmaps (O/RO/R/RU/U/LU/L/LO.TGA)
// selected by snapping oCNpc::GetAngles yaw to 45-degree sectors (boundaries
// 22.5/67.5/112.5/157.5); OpenGothic rotates a single U.TGA continuously.
