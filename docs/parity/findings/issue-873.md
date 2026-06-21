# Issue #873 — b, c, m keys not working sometimes

- Category: input (X11 key mapping)
- Disposition: **DEFER** (root cause is in the Tempest engine submodule, not OpenGothic game code; rule forbids editing engine source)

## Problem
On Arch + i3, keys b/c/m (map / tasks / abilities) intermittently stop working
until restart; arrow keys keep working. Heisenbug; gone under a debugger.

## Root cause (confirmed by reporter in-issue)
The X11 backend translated certain physical keys (notably backtick `` ` `` and
the `Super_L`/`Super_R` modifiers used by tiling WMs) to `Event::NoKey`. An
unmapped key produced a `NoKey` event that drove an infinite/stuck loop in the
event-dispatch path, after which subsequent letter-key events (b/c/m) were
dropped — arrows survived because they are mapped. Adding the missing mappings
(`XK_Super_L`/`XK_Super_R`, backtick, etc.) made the bug disappear over a week of
play.

## OG files (game side — for reference only)
- `game/utils/keycodec.cpp` — `keyToCode()`/`tr()` already returns 0/`Idle` for
  unknown codes; this layer is NOT the loop source.
- The actual fix lives in the Tempest engine:
  - `lib/Tempest/Engine/system/api/x11api.cpp` (key translation table; add
    `XK_Super_L -> K_LSuper`, `XK_Super_R -> K_RSuper`, backtick, and any other
    keys that currently fall through to `NoKey`)
  - `lib/Tempest/Engine/system/eventdispatcher.cpp` (should treat a `NoKey`
    translation as "drop event", never as a loop/retry condition)

## Divergence
This is not a Gothic-parity divergence; it is an input-backend completeness bug
in Tempest. OpenGothic game code is correct.

## Recommendation
- Treat as **upstream Tempest** fix. Two independent hardening steps:
  1. Complete the X11 keysym table (add Super L/R and backtick at minimum).
  2. Make `eventdispatcher` robust to `NoKey` so an unmapped key can never wedge
     the dispatch loop (defensive — fixes the whole class, not just these keys).
- No OpenGothic `game/` patch is appropriate here. Per the hard rules (do not
  edit engine source) this is left as DEFER with the above guidance; the reporter
  already has a working local patch matching item (1).
