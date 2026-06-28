# Inventory grid: `invMaxColumns<=0` sentinel and missing fit-to-width column clamp

**Confidence:** Medium (the code divergence is certain/high-confidence; it only *manifests*
under non-default `Gothic.ini` settings or unusually narrow render targets, so player-visible
impact is config-gated).

## Original fn + address

`oCItemContainer::Draw` @ `0x007076b0` (and the parallel setup in `oCItemContainer::Init`
@ `0x00706870`) computes the grid column count in three layered steps:

1. It reads `GAME/invMaxColumns` via `zCOption::ReadInt(..., s_invMaxColumns, 5)` (default 5)
   and caches it. Crucially, a configured value of **0 (or any negative value) is treated as a
   sentinel**: the cache is stamped with `0xffff` and the working column count is set to `0x400`
   (1024). A positive value is used verbatim.
2. Independently it derives the maximum number of columns that physically fit the container's
   virtual width: `maxFit = (0x2000 - cellW) / cellW` (i.e. `8192/cellW - 1`, floored to a
   minimum of 2, and halved when the screen is split). `0x2000` is the full 8192-unit virtual
   view width and `cellW` is `DAT_00ab0fa8`.
3. It then **clamps**: `columns = min(iniColumns, maxFit)`. So `invMaxColumns=0` means "use as
   many columns as fit the width," and even a positive `invMaxColumns` is capped so the grid can
   never overflow the available width. (`invMaxRows`/`s_invMaxRows` is handled symmetrically for
   rows, default 0 → `0x400` → clamped to the rows that fit the height.)

## OG file:line

`game/ui/inventorymenu.cpp:85-88` (column count) and `game/ui/inventorymenu.cpp:406-409`
(`rowsCount`); the fixed value is consumed unclamped at `drawAll` `:582-583`.

```cpp
int invMaxColumns = Gothic::settingsGetI("GAME","invMaxColumns");
if(invMaxColumns>0)
  columsCount = size_t(invMaxColumns); else
  columsCount = 5;
```

## Divergence

Two related differences, both in the grid-cell-count subsystem:

1. **Sentinel mapping.** Original maps `invMaxColumns <= 0` to "auto-fill the width" (1024 then
   clamped to `maxFit`). OpenGothic maps `<= 0` to a hardcoded **5**. A user who sets
   `invMaxColumns=0` (a documented "auto/max columns" tweak) gets a wide auto-fitted grid in the
   original but a fixed 5-column grid in OpenGothic.

2. **Missing fit-to-width clamp.** Original always applies `columns = min(ini, maxFit)`, so the
   column count is bounded by what fits. OpenGothic uses `columsCount` verbatim with no
   width-based clamp anywhere (`columsCount` is set only at `:85-88`; `drawAll` consumes it
   directly). A large positive `invMaxColumns` (or a small render target / large
   `inventoryCellSize`) overflows the right-aligned player grid off the left edge in OpenGothic,
   whereas the original silently reduces the column count to fit. (`invMaxRows` is likewise never
   read — `rowsCount()` is purely height-derived, so the row analogue of the sentinel/cap is also
   absent, though height-derivation incidentally approximates the original's row clamp.)

Under the shipped default (`invMaxColumns=5` on any normal-width screen) both behave identically,
which is why this is config-gated rather than always-on.

## Proposed patch

**DEFERRED.** A faithful fix is not surgical. Reproducing the original requires computing the
per-container `maxFit = availableWidth/cellW` and applying `columsCount = min(configured, maxFit)`
(with the `<=0` case meaning "use `maxFit`"). OpenGothic's layout draws the two grids
right-/left-aligned at a fixed `padd` with the grid width chosen *from* `columsCount`
(`drawAll` `:599-600`), so there is no single "available width per container" value equivalent to
the original's `0x2000` virtual span to clamp against without restructuring `drawAll`'s geometry.
A naive clamp using `w()` would not match the original's split-screen halving or its virtual-width
formula, risking a new divergence. Recommend deferring until the inventory layout is reworked to
carry an explicit per-page available width.

// NOTE: in original-game oCItemContainer::Draw @0x007076b0 / oCItemContainer::Init @0x00706870 the
// column count is min(invMaxColumns, (0x2000-cellW)/cellW), and invMaxColumns<=0 is a sentinel
// meaning "fill the available width". OpenGothic hardcodes 5 for invMaxColumns<=0 and never clamps
// the configured column count to what fits the render width.
