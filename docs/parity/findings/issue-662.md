# Issue #662 — Hold-space equipped-spell switcher menu

- Category: UI / input (feature)
- Disposition: **DEFER** (new UI widget + input-mode work; not a surgical fix)

## Problem
In the original, long/continuous hold of SPACE opens an equipped-spell switcher
ring (spell icons, quick-slot numbers, rune/scroll/item indicators, A/D to
cycle). In OG, long-pressing space is interpreted as repeated taps and just
spam-redraws the last weapon.

## OG files
- `game/utils/keycodec.cpp` — SPACE maps to `keyWeapon` (code `0x3900`), action
  `Weapon` (default keys, lines 53, 202/230).
- `game/game/playercontrol.cpp` — `onKeyPressed` Weapon handling (lines 73-84):
  toggles weapon draw/close and uses `wctrlLast` to redraw last weapon. There is
  no hold-duration tracking and no spell-ring state.
- `game/mainwindow.cpp:469 keyRepeatEvent` — repeats are routed to menus/inventory
  but there is no game-side "hold detector" for SPACE.

## Divergence
No equipped-spell switcher exists at all. Hold-SPACE is treated as discrete
Weapon presses (draw/close). `wctrlLast` (playercontrol.h:142) only remembers the
last weapon slot; there is no ring UI, no A/D cycling within it, no per-slot spell
preview.

## Why DEFER (not FIX)
This is a genuinely new feature requiring:
1. A hold-timer on SPACE in the player-control/input layer (distinguish tap =
   draw weapon vs. hold = open switcher), gated so it does not break the existing
   weapon-toggle on a quick tap.
2. A new selection widget (icons for equipped spells from
   `Inventory::currentSpell` / mana/rune slots), with A/D (Left/Right) cycling
   that updates the active spell slot (`wctrl[id]` / `wctrlLast`).
3. Suppression of the spam weapon-draw while the ring is open.

Logic is entirely OG-side; no original-binary decompile is required for parity,
but it is too large/behavioral for a surgical patch. Marked **good first issue**
upstream — appropriate as a guided contribution, not an immediate fix.

## Pointers for an implementer
- Detect hold in `PlayerControl::onKeyPressed`/`onKeyReleased` for `Action::Weapon`
  + a tick threshold; only commit the weapon-draw on release-if-short.
- Build the ring from `pl->inventory()` equipped spells; reuse
  `wctrl[]`/`wctrlLast` to apply the chosen slot.
- Reuse menu input routing patterns from `game/ui/` for the icon strip.
