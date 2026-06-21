# Issue #713 — Enabling / disabling subtitles

- Category: UI / settings
- Disposition: **DEFER** (core toggle already implemented; remaining ambient/noise toggles are a small additive task)

## Problem
Request to toggle subtitles on/off (the original exposes 4 flags:
`subTitles`, `subTitlesPlayer`, `subTitlesAmbient`, `subTitlesNoise`).

## Current state in OG (already done)
- Defaults registered: `game/gothic.cpp:128-129` (`subTitles`, `subTitlesPlayer`).
- Dialog gating implemented: `game/ui/dialogmenu.cpp`
  - `setupSettings()` reads `showSubtitles` / `showSubtitlesPlayer` (lines 57-60).
  - `haveToShowSubtitles(bool isPl)` (lines 379-381):
    `return showSubtitles && (showSubtitlesPlayer || !isPl);`
  - Used to gate dialogue rendering at `dialogmenu.cpp:420`.
- Per maintainer comment, `GAME/subTitles` landed via PR #809.

So the headline request (disable subtitles) is satisfied for dialogue lines.

## Remaining gap (the DEFER work)
The two "ambient" flags are not wired:
- `subTitlesAmbient` — gates ambient/background SVM speech printed via
  `Npc::aiOutputSvm` / `GameScript::aiOutputSvm` (game/game/gamescript.cpp:1285)
  and the non-dialogue print path `DialogMenu::print` (dialogmenu.cpp:312) /
  `onPrintScreen` (dialogmenu.cpp:247).
- `subTitlesNoise` — gates noise/SVM "noise" lines.

## Guidance for a FIX (kept as DEFER pending behavior confirmation)
1. Register defaults in `game/gothic.cpp` next to the existing two:
   `defaults->set("GAME","subTitlesAmbient",1); defaults->set("GAME","subTitlesNoise",1);`
2. In `DialogMenu::setupSettings()` read them into members
   `showSubtitlesAmbient`, `showSubtitlesNoise`.
3. Gate the ambient/print path: in `DialogMenu::print()` (and/or the SVM-overlay
   branch in `aiOutput`) early-return when the line is ambient/noise and the
   corresponding flag is 0. The hard part is classifying a given output as
   "ambient" vs "noise" vs normal dialogue — this requires tracing how
   `aiOutputSvm(overlay=true)` and `ai_printscreen` map to these categories at
   runtime, which is why this is DEFER rather than an immediate surgical patch.
   No original-binary lookup is needed; the categorization is OG-side and should
   be confirmed by runtime testing before committing exact gates.

## Note
A contributor (lezeSoftware) already volunteered in-issue. Recommend pointing
them at `dialogmenu.cpp:setupSettings`/`print` and `gamescript.cpp:aiOutputSvm`.
