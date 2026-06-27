# PrintScreen timed messages stack/overlap instead of being replaced in place

**Confidence:** Medium-High

## Original function + address (prose only)

The script externals `PrintScreen` (`oCGame` external dispatcher at `0x6e2da0`,
in `oGameExternal.cpp`) and `AI_PrintScreen` (`oCNpc::EV_PrintScreen`, the
message-event handler) both end in the same dispatch: depending on whether the
script-supplied `posx`/`posy` are `-1`, they call one of
`zCView::PrintTimed(x,y,...)`, `zCView::PrintTimedCX(y,...)`,
`zCView::PrintTimedCY(x,...)` or `zCView::PrintTimedCXY(...)` on the screen view
held at `ogame+0x30`, always with `duration = timesec*1000` and a null `zCOLOR*`
(default font colour). `zCView::PrintTimed` lives at `0x7a7da0`; the CX/CY/CXY
variants follow it.

All four variants funnel into `zCView::CreateText(int x, int y, const zSTRING&,
float dur, zCOLOR&, ...)`. The decisive detail is `CreateText`'s body: before
allocating a new `zCViewText`, it walks the view's existing timed-text list
(head pointer at `this+0x88`) and, for every existing node, compares the node's
stored `x` (offset +4) and `y` (offset +8) against the requested `x,y`. If a
node with the **same (x,y)** already exists, that node is **reused** — its
string, duration, colour and timer are overwritten in place — and no new node is
created. Only when no node matches that exact position is a fresh node inserted
at the head of the list. In other words, the original maintains at most one
on-screen timed message per unique screen coordinate, and a repeated print at
the same coordinate replaces the text and resets the timer.

(For the centered paths CX/CY/CXY the stored x/y are the post-centering virtual
coordinates, so two messages only merge when their computed positions coincide;
for the explicit-coordinate path the stored x/y are the raw script values.)

## OpenGothic file:line

`game/ui/dialogmenu.cpp:287-299` — `DialogMenu::printScreen(...)`.
Aging/removal: `game/ui/dialogmenu.cpp:109-117`. Draw: `:450-475`.
Struct: `game/ui/dialogmenu.h:95-101` (`PScreen{ txt, font, time, x, y }`),
`pscreen` is an unbounded `std::vector<PScreen>` (`dialogmenu.h:141`).

## Divergence

`DialogMenu::printScreen` unconditionally does
`pscreen.emplace(pscreen.begin(), std::move(e))` — every call pushes a brand-new
entry. There is no lookup-by-position, so repeated `PrintScreen`/`AI_PrintScreen`
calls at the same `(x,y)` accumulate independent, fully overlapping entries that
each age out on their own timer (`tick`, lines 109-117). The original keeps a
single entry per coordinate and refreshes it.

Observable effect: a script that prints to a fixed coordinate repeatedly (e.g.
a per-frame/per-tick HUD-style readout, a counter, or a status line that is
re-issued before the previous one expires) renders cleanly as one updating line
in the original, but in OpenGothic produces N superimposed copies drawn at the
same pixel (garbled bold-looking text) that linger for their full duration,
with the `pscreen` vector growing unbounded until the timers drain.

## Proposed patch

Replace-in-place when an explicit-coordinate message already occupies the same
`(x,y)`. This reproduces the original's behaviour for the explicit-position case
(the common and unambiguous one) and leaves the centered (`x<0`/`y<0`) cases
exactly as they are today (those already overlap in both engines).

OLD (`game/ui/dialogmenu.cpp`, `DialogMenu::printScreen`):
```cpp
  e.time = (time>0) ? uint32_t(time*1000) : 0u;
  e.x    = x;
  e.y    = y;
  pscreen.emplace(pscreen.begin(),std::move(e));
  update();
```
NEW:
```cpp
  e.time = (time>0) ? uint32_t(time*1000) : 0u;
  e.x    = x;
  e.y    = y;
  // NOTE: in original-game zCView::CreateText (reached via the PrintScreen
  // external @0x6e2da0 and oCNpc::EV_PrintScreen) timed messages are keyed by
  // their (x,y): a print at an already-occupied coordinate REUSES that slot
  // (text+timer overwritten) instead of stacking. Match that for explicit
  // coordinates; centered (-1) prints keep stacking as before.
  if(x>=0 && y>=0) {
    for(auto& it:pscreen)
      if(it.x==x && it.y==y) {
        it = std::move(e);
        update();
        return;
        }
    }
  pscreen.emplace(pscreen.begin(),std::move(e));
  update();
```

Grep-verified symbols: `DialogMenu::printScreen` (`dialogmenu.cpp:287`),
`pscreen` / `PScreen{txt,font,time,x,y}` (`dialogmenu.h:95,141`), `update()`
(member, used throughout `dialogmenu.cpp`). No new fields introduced;
assignment of a `PScreen` (trivially copy/move-assignable) is valid.
