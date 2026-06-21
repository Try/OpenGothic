# Issue #799 — Command-line argument compatibility with the original game

**Category:** CLI · **Disposition:** DEFER (partial; umbrella issue)

## Intended behavior (original Gothic II)
The original `Gothic2.exe` accepts a large set of CLI flags (collected by the
maintainer from GothicModComposer), e.g.:
- Game manager: `-nomenu` (skip menu, start new game)
- Startup: `-zRes <w> <h> <bpp>`, `-zWindow`
- Audio: `-zNoMusic`, `-zNoSound`
- Dev: `-devmode` (MARVIN), `-game <file>` (load a mod `.ini`)
- Engine/perf: memory pool, texture-conversion, rendering toggles

## OpenGothic — current state
Arg parsing lives in `game/commandline.cpp` (`CommandLine::CommandLine`,
lines 43-156). Already mapped (with OpenGothic-native spelling):
- `-nomenu` (l.83), `-window` (l.80), `-devmode` (l.61)
- `-game:<file>` (l.51) — note: colon form, not the original `-game <file>`
- `-g <path>`, `-save`, `-w`, `-g1/-g2/-g2c`, `-benchmark`, plus renderer flags.

Unknown args fall through to a log line (`Log::i("unreacognized commandline
option…")`, l.154) and are otherwise ignored — so the original's `-z*` flags
are silently dropped.

## Gap
This is an umbrella/tracking issue. Genuinely-small, low-risk additions that
map cleanly onto existing OpenGothic state:
- `-zWindow`  → alias of existing `-window` (sets `isWindow`).
- `-zNoMusic` → set `SOUND/musicEnabled=0` override.
- `-zNoSound` → set `SOUND/soundEnabled=0` override (see #899).
- `-game <file>` (space form) → alias of `-game:` (mod ini).

`-zRes <w> <h> <bpp>` and the engine/memory flags need resolution/renderer
plumbing and should be triaged separately; many engine flags are obsolete
(zEngine memory pools do not exist in OpenGothic).

## Recommendation
DEFER as a tracking issue. Land the cheap aliases above as individual PRs in
`game/commandline.cpp`. The `soundEnabled`/`musicEnabled` plumbing they need
already exists (see issue-899 findings). No single surgical patch closes #799.
