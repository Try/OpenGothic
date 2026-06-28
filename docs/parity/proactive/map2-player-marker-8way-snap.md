# Map player-marker continuous rotation vs original 8-way discrete arrow snap

**Confidence:** Medium-High (divergence existence is HIGH-confidence from the decompile; the
exact quantization patch is Medium because it assumes OpenGothic's already-shipped base marker
orientation is correct and only needs to be snapped).

## Original function + address (prose)

`oCViewDocumentMap::UpdatePosition` @ `0x0068d7b0` (P:\dev\g2addon\release\Gothic\_roman\oViewDocumentMap.cpp)
is what draws the player-position marker on the open map. It does **not** rotate a single marker
sprite. Instead:

1. It builds a world-space reference point = player position offset by +1 on world Z (north):
   `(posX, posY, posZ + 1.0)` and calls `oCNpc::GetAngles` @ `0x006812b0`, which returns a signed
   heading angle (degrees, range -180..180) between the player's facing/at-vector and world north.
2. It buckets that heading into **eight 45-degree sectors**. The float comparison constants in the
   function decode exactly to the sector boundaries `±22.5`, `±67.5`, `±112.5`, `±157.5`
   (`0x41b40000`=22.5, `0x42870000`=67.5, `0x42e10000`=112.5, `0x431d8000`=157.5), i.e. eight
   buckets centered on multiples of 45 degrees.
3. The bucket index (0..7) selects one of eight **pre-oriented arrow textures** whose names are
   assembled at runtime as a direction token + ".TGA": `O`, `RO`, `R`, `RU`, `U`, `LU`, `L`, `LO`
   (German screen directions Oben/Rechts/Unten/Links and the diagonals) → `O.TGA`, `RO.TGA`,
   `R.TGA`, `RU.TGA`, `U.TGA`, `LU.TGA`, `L.TGA`, `LO.TGA`. The bare tokens (`RU`@0x008b0da8,
   `LU`@0x008b0da4, `U`/`O`...) and the suffix exist as static strings; the combined filenames are
   built per-frame (none of `RO.TGA`/`RU.TGA`/etc. exist as static strings, confirming runtime
   concatenation). `U.TGA`@0x008b6d38 and `O.TGA`@0x008b6d30 are the only two that happen to also
   exist standalone.
4. The chosen sprite is placed with `SetPixelPosition` and drawn **without any rotation** — the
   sprite art itself already points in its compass direction.

Net effect in the original: the on-map player arrow **snaps** to one of 8 fixed orientations.

## OpenGothic file:line

`game/ui/documentmenu.cpp:14` loads a single fixed texture:
`cursor = Resources::loadTexture("U.TGA");`

`game/ui/documentmenu.cpp:111` then draws it with a **continuous** rotation:
`p.rotate(-pl->rotation()-90);`

## Divergence

OpenGothic loads exactly one of the original's eight direction sprites (`U.TGA`, the down/"Unten"
arrow) and rotates it smoothly by the live player heading. The original never rotates the sprite;
it quantizes the heading to 45-degree steps and swaps among 8 pre-drawn arrows. The player-visible
difference: in the original the map arrow ticks through 8 discrete headings; in OpenGothic it
glides continuously. This is a behavioral parity gap in the map player-marker heading.

## Proposed patch

Quantize OpenGothic's existing marker orientation to the nearest 45 degrees, reproducing the
original's 8-way snap while reusing OpenGothic's already-shipped base orientation/sign (so the fix
does not depend on resolving the `GetAngles` vs `Npc::rotation()` sign convention). `round(x/45)*45`
snaps to multiples of 45, which is exactly the bucket centering implied by the original's
±22.5/±67.5/±112.5/±157.5 boundaries.

`game/ui/documentmenu.cpp` (add `#include <cmath>` near the top if not already pulled in transitively):

OLD (line 111):
```cpp
      p.rotate(-pl->rotation()-90);
```

NEW:
```cpp
      // NOTE: in original-game oCViewDocumentMap::UpdatePosition @0x0068d7b0 the player marker is
      // NOT continuously rotated: oCNpc::GetAngles @0x006812b0 yields the heading vs world-north,
      // which is bucketed into eight 45-deg sectors (boundaries 22.5/67.5/112.5/157.5) selecting
      // one of 8 pre-oriented arrow sprites (O/RO/R/RU/U/LU/L/LO + ".TGA") drawn unrotated.
      // Reproduce the discrete snap by quantizing our orientation to the nearest 45 degrees.
      p.rotate(std::round((-pl->rotation()-90)/45.f)*45.f);
```

This is surgical (one expression), build-verifiable, and changes only the marker's heading from
continuous to 8-step discrete, matching the original's observable behavior.
