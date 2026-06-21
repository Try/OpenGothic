# Wld_ExchangeGuildAttitudes / guild-attitude table default fill is HOSTILE instead of the engine's NEUTRAL/FRIENDLY (value 3)

**Confidence:** High

## Original function + address

`oCGuilds::oCGuilds()` constructor in `Gothic2.exe` at `0x00700c30`.
The guild count is hard-set to `0x42` (66 = `GIL_PUBLIC`). It allocates the
attitude table as `gilCount * gilCount` bytes (`0x1104` = 4356) and, before any
script data is loaded, fills **every byte of the table with the value `0x03`**
(the constructor memset-style loop writes `0x03030303` words plus a `0x03`
tail). Only afterward does `InitGuildTable(this, "GIL_ATTITUDES")` overlay the
script-provided attitudes.

The script array `GIL_ATTITUDES` is only `TAB_ANZAHL` entries
(`TAB_ANZAHL = 4096`, so a `64 x 64` block), while the engine table is
`GIL_MAX x GIL_MAX = 66 x 66`. Guild indices `64` and `65` (e.g. the
ambient-monster guild and `GIL_PUBLIC`) therefore fall **outside** the script
overlay and keep the constructor's default fill of `3` for the rest of the run.

For reference, `oCGuilds::GetAttitude` (`0x00700d40`) returns the literal `2`
only for the *out-of-range guild* error path (the already-fixed
`ATT_NEUTRAL`/`2` case); the in-range default that NPC attitude logic actually
reads is the constructor's fill value `3`.

## OpenGothic file:line

`game/game/gamescript.cpp:388`

```cpp
gilAttitudes.resize(gilCount*gilCount,ATT_HOSTILE);
```

`gilCount` is `GIL_MAX` (66) and `gilTblSize` is `sqrt(TAB_ANZAHL)` (64);
`wld_exchangeguildattitudes("GIL_ATTITUDES")` (gamescript.cpp:1708-1716) only
overwrites the top-left `gilTblSize x gilTblSize` (64x64) sub-block, leaving the
remaining rows/columns (guilds 64 and 65) at the resize default.

## Divergence

OpenGothic initializes the entire `gilAttitudes` table to `ATT_HOSTILE` (integer
`0`). The original engine initializes the entire table to integer `3`
(`ATT_FRIENDLY` in OpenGothic's `enum Attitude`, constants.h:248). Because the
`GIL_ATTITUDES` script overlay covers only the 64x64 block, every guild pair
involving guild index 64 or 65 (e.g. `GIL_PUBLIC`) resolves to:

- Original: attitude `3` (friendly/non-hostile default)
- OpenGothic: attitude `0` (`ATT_HOSTILE`)

This makes NPCs of / toward those high-index guilds spuriously hostile compared
to the original game.

## Proposed patch

```cpp
// OLD
gilAttitudes.resize(gilCount*gilCount,ATT_HOSTILE);

// NEW
// NOTE: in original-game oCGuilds::oCGuilds (Gothic2.exe 0x00700c30) the full
// gilCount*gilCount attitude table is initialized to integer 3 (ATT_FRIENDLY)
// before the GIL_ATTITUDES script overlay; guild pairs outside the 64x64
// TAB_ANZAHL block keep this default. OpenGothic used ATT_HOSTILE (0), making
// high-index guilds (>=64, incl. GIL_PUBLIC) spuriously hostile.
gilAttitudes.resize(gilCount*gilCount,ATT_FRIENDLY);
```

Grep-verified symbols: `gilAttitudes`, `gilCount`, `gilTblSize`
(gamescript.h:485-487, gamescript.cpp:384-388); `ATT_FRIENDLY = 3` and
`ATT_HOSTILE = 0` (constants.h:245,248). Single init site confirmed
(`gilAttitudes.resize` appears once).
