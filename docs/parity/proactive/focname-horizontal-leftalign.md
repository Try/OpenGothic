# Focus name-plate horizontal placement: vanilla left-aligns, OpenGothic centers

**Confidence:** Medium-High. The divergence is certain from the binary (zCView::Print is the
left-aligned printer, not PrintCX, and the X clamp treats the coordinate as a left edge); the
rating is tempered only because horizontal name placement is cosmetic and OpenGothic may have
centered deliberately.

## Original fn + address

`oCGame::UpdatePlayerStatus` @ `0x006c3140` (P:\dev\g2addon\release\Gothic\_ulf\oGame.cpp) is the
routine that draws the focus name-plate every frame. After it resolves the focused vob's display
name (mob via `oCMOB::GetName`, NPC via `oCNpc::GetName`, item via `oCItem::GetName`), it computes a
single anchor point from the focused vob's bounding box:

- anchor X/Z = horizontal center of the vob `((bbMax + bbMin) * 0.5)` (or, for model vobs, the vob's
  world-pivot translation), i.e. the model's horizontal center;
- anchor Y = near the top of the bbox (≈0.82 of the box height above center).

That anchor is projected through `zCCamera::activeCam` to a screen pixel, converted to the view's
virtual coordinate space via `zCView::anx`/`zCView::any`, then clamped and drawn with
**`zCView::Print(x, y, name)`** @ `0x007a9a40`.

Two facts pin down the alignment:
1. The routine calls `zCView::Print` (the **left-aligned** printer that draws from `x` as the left
   edge via `PrintChars`), **not** `zCView::PrintCX`/`PrintCXY` (the centered variants).
2. The text width from `zCView::FontSize(name)` is used **only** for the right-edge screen clamp
   (`if (0x1fff - textWidth < x) x = 0x1fff - textWidth`). It is **never** subtracted from `x` to
   center the string.

So the original puts the name's **left edge at the focused object's horizontal center** and clamps
it on-screen: X ∈ `[0, screenW - textWidth]`, Y ∈ `[fontHeight, screenH - fontHeight]` (virtual
units, screen = 0x2000).

## OG file:line

`game/mainwindow.cpp:584` `MainWindow::paintFocus(...)`, specifically lines 603-613:

```cpp
int   ix  = int((0.5f*pos.x+0.5f)*float(w()));
int   iy  = int((0.5f*pos.y+0.5f)*float(h()));
auto& fnt = Resources::font(scale);

auto tsize = fnt.textSize(focus.displayName());
ix-=tsize.w/2;            // centers the name on the projected anchor
if(iy<tsize.h)
  iy = tsize.h;
if(iy>h())
  iy = h();
fnt.drawText(p,ix,iy,focus.displayName());
```

## Divergence

`ix-=tsize.w/2;` horizontally **centers** the focus name over the projected anchor. The original
**left-aligns** it (anchor X = left edge). The two placements differ by a constant `textWidth/2`
rightward shift in the original relative to OpenGothic — visible on every name-plate, every frame.

Secondary, same-routine gaps: OpenGothic has **no horizontal on-screen clamp** at all (a name near
the left/right screen edge can run off-screen, whereas vanilla clamps X into `[0, w-textWidth]`),
and its bottom Y clamp is `h()` rather than vanilla's `h()-fontHeight` (so the last line can drop a
font-height too low at the very bottom).

## Proposed patch

```cpp
// OLD
auto tsize = fnt.textSize(focus.displayName());
ix-=tsize.w/2;
if(iy<tsize.h)
  iy = tsize.h;
if(iy>h())
  iy = h();
fnt.drawText(p,ix,iy,focus.displayName());

// NEW
// NOTE: in original-game oCGame::UpdatePlayerStatus @0x006c3140 the focus name-plate is drawn
// with zCView::Print (left-aligned at x), NOT PrintCX; the projected anchor X is the focused
// object's horizontal center and is used as the text's LEFT edge (no textWidth/2 centering).
// The string is then clamped fully on-screen: x in [0, w-textWidth], y in [fontH, h-fontH].
auto tsize = fnt.textSize(focus.displayName());
if(ix<0)
  ix = 0;
if(ix>w()-tsize.w)
  ix = w()-tsize.w;
if(iy<tsize.h)
  iy = tsize.h;
if(iy>h()-tsize.h)
  iy = h()-tsize.h;
fnt.drawText(p,ix,iy,focus.displayName());
```

Build-verifiable, surgical, no new symbols (`w()`, `h()`, `tsize`, `ix`, `iy` all already in scope).

### Investigated but NOT included (kept out to avoid false positives)
- **`*(oCNpc+0x43c) != 0` suppresses the focus name AND HP bar.** The original gates the entire
  NPC name-plate (name + HP bar) on this oCNpc field being zero, in addition to the already-fixed
  `HP>0` bar condition. Could not resolve the semantic of oCNpc offset 0x43c from the warm
  decompiler, and OpenGothic has no corresponding state to gate on, so this fails the
  "concrete condition/value" bar — DEFERRED.
- **" (locked)" suffix on the focus name** (appended when `oCNpc::s_bTargetLocked != 0`, the
  Ghidra label `s___locked_` decodes to the C-string " (locked)"). No writer of `s_bTargetLocked`
  was found in the melee/strafe AI, suggesting it is a dev/debug global that retail never sets;
  unverifiable reachability — DEFERRED.
