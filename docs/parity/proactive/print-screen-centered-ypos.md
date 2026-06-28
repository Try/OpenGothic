# PrintScreen centered (posy == -1) text sits one line too high

Confidence: Medium-High

## Original function + address
The script external `PrintScreen` builds an `oCMsgConversation` that is dispatched
by `oCNpc::EV_PrintScreen` (Ghidra `0x759270`). When posx and/or posy are `-1`,
EV_PrintScreen routes to the centering helpers `zCView::PrintTimedCXY`
(`0x7a7fc0`), `PrintTimedCX` (`0x7a7db0`) or `PrintTimedCY` (`0x7a7f00`) instead
of `zCView::PrintTimed` (`0x7a7d20`). For the centered axis those helpers compute
the coordinate as `(0x2000 - fontExtent)/2` in the 0..0x2000 (==8192) virtual
space: e.g. PrintTimedCXY passes `y = (0x2000 - GetFontY())/2`, i.e. the **top
edge** of the text block, so that the block (height = one font line) is centered
about the screen mid-line — top at `(viewH - lineH)/2`, bottom at
`(viewH + lineH)/2`, visual center at `viewH/2`. `zCView::CreateText`
(`0x7a7ab0`) stores that value as the text top (offset 0x08) and the view blits
the glyphs downward from it.

## OG file:line
`game/ui/dialogmenu.cpp:479-480` (centered branch of the `pscreen` draw loop),
together with `GthFont::drawText` treating its `y` as the **bottom** of the line
(`int y = by - h;`, `game/utils/gthfont.cpp:114`) and `GthFont::textSize`
returning `sz.h == pixelSize()` for a single line (`game/utils/gthfont.cpp:147,175`).

## Divergence
`drawText(p, x, y, txt)` draws glyphs into `[y - sz.h, y]`, so the supplied `y`
is the text **bottom**, not the top. The earlier explicit-posy fix correctly
accounts for this by adding `sz.h` (top -> bottom) on line 485. The centered
branch (line 480) was left as `y = (area.h - sz.h)/2` and feeds that straight to
`drawText` as the bottom. The glyph block therefore lands at
`[(area.h - 3*sz.h)/2, (area.h - sz.h)/2]`, whose visual center is
`area.h/2 - sz.h` — a full line height **above** the true center the original
produces (`area.h/2`). The X centered branch (line 475, `(area.w - sz.w)/2`) is
correct because `drawText`'s `x` is the left edge, so the same formula shape is
right for X but wrong for Y, where the anchor is the opposite (bottom) edge.
Net effect: every fully-centered `PrintScreen(text, -1, -1, font, time)` renders
one line of text height too high versus Gothic2.exe.

Medium-High: the offset is exact (`sz.h`) and derivable from the helpers above;
residual uncertainty is only in how often scripts use the centered form rather
than explicit percentages.

## Proposed patch
File: `game/ui/dialogmenu.cpp`

OLD:
```cpp
    if(y<0){
      y = (area.h-sz.h)/2;
      } else {
```
NEW:
```cpp
    if(y<0){
      // NOTE: in original-game zCView::PrintTimedCXY/CY @0x7a7fc0/0x7a7f00 the centered text
      // TOP is (viewH-lineH)/2 (block centered on viewH/2). GthFont::drawText anchors on the
      // line bottom (y=by-h), so add sz.h to convert top->bottom; without it the block sat a
      // full line too high (visual center at area.h/2-sz.h instead of area.h/2).
      y = (area.h-sz.h)/2 + sz.h;
      } else {
```
