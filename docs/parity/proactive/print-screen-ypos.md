# PrintScreen Y percentage uses wrong anchor (offset by text height)

Confidence: Medium

## Original function + address
Script external `PrintScreen` (`oGameExternal.cpp`, Ghidra `FUN_006e2da0`) and
`oCNpc::EV_PrintScreen` convert the posx/posy script percentages into the
ZenGin virtual coordinate space (0..0x2000 == 0..8192) and pass them straight to
`zCView::PrintTimed(x, y, ...)`. `zCView::CreateText` stores that `y` as the
**top edge** of the text (offset 0x08 of the `zCViewText`); the view blits the
text downward from that top. So with an explicit posy the original places the
text TOP at `viewH * posy/100`. The X axis is the analogous left edge
`viewW * posx/100`. The dedicated center helpers (`PrintTimedCY/CXY`) instead
use `(0x2000 - fontHeight)/2`, i.e. they center the text block.

## OG file:line
`game/ui/dialogmenu.cpp:455-466` (drawing of `pscreen`), with
`GthFont::drawText` treating its `y` as the **baseline** (`y = by - h`,
`game/utils/gthfont.cpp:114`).

```cpp
if(y<0){
  y = (area.h-sz.h)/2;
  } else {
  y = ((area.h-sz.h)*y)/100+sz.h;
  }
```

## Divergence
Because `drawText` subtracts the glyph height, the effective text TOP for an
explicit posy becomes `((area.h - sz.h) * posy)/100`, whereas the original is
`area.h * posy/100`. The two differ by `sz.h * posy/100`: at posy=100 the
original pushes the text fully off the bottom edge while OG keeps it flush; at
posy=50 OG sits half a line higher than the original. X (line 460) and the
center cases (posx/posy == -1) already match the original; only the explicit-Y
branch is off. Medium confidence: correct for the common `-1,-1` centered case,
visibly wrong only for scripts that pass an explicit posy.

## Proposed patch
File: `game/ui/dialogmenu.cpp`

OLD:
```cpp
    if(y<0){
      y = (area.h-sz.h)/2;
      } else {
      y = ((area.h-sz.h)*y)/100+sz.h;
      }
```
NEW:
```cpp
    if(y<0){
      y = (area.h-sz.h)/2;
      } else {
      // NOTE: in original-game (zCView::CreateText) posy is the text TOP at
      // viewH*posy/100; GthFont::drawText anchors on the baseline, so add sz.h
      // to convert top -> baseline without the extra (area.h-sz.h) scaling.
      y = (area.h*y)/100 + sz.h;
      }
```
