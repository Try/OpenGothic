# Dive/breath bar: the original bar DOES drain — OpenGothic is already correct; the "always-full" parity patch is wrong

**Confidence:** High (backed by direct Gothic2.exe field-write evidence). This is a *correction*
of an earlier proactive doc, not a new OpenGothic bug: OpenGothic's draining dive bar is faithful
to the original, and the previously-proposed "render permanently full" patch would INTRODUCE a
divergence. Net deliverable for OpenGothic code: **DEFERRED / no change — keep current behavior.**

## Original function + address (prose only)

The per-frame NPC breath/drown handler is an un-named `oCNpc` member occupying the gap between
`oCNpc::UpdateNextVoice` (`0x0073e3c0`, size 183, ends `0x0073e477`) and
`oCNpc::oCNpcTimedOverlay::GetMdsName` (`0x0073e8d0`). Ghidra never promoted this region to a
defined `Function` (`getFunctionContaining(0x0073e58b)` returns null), so any analysis that
enumerates *defined functions only* — `wde offsets`, the demangled-function table, and the
"complete `.text` scan" cited in `statbar-divebar-never-drains.md` — silently skips it.

Two `oCNpc` float fields drive the dive bar and the drown timer:
- `+0x7d0` = configured dive-time max = `dive_time * 1000` (ms). Written by
  `oCNpc::SetSwimDiveTime` (`0x00741f70`, multiplier `0x447a0000` = 1000.0).
- `+0x7d4` = the *live* breath countdown (ms), initialised to `+0x7d0`.

The bar reader `oCGame::UpdatePlayerStatus` (`0x006c3140`) feeds the BAR_MISC fill via
`oCNpc::GetSwimDiveTime` helper (`0x00741fa0`) as `value=npc[0x7d4]`, `rangeMax=npc[0x7d0]`, i.e.
fill = `0x7d4 / 0x7d0`.

**Hard evidence that `+0x7d4` is decremented every frame (the fact the prior doc missed).**
`wde find 0x7d4` reports these write/read sites — note they come back as *bare addresses with no
owning function*, exactly because they sit inside the undefined breath handler:

- drain branch (DIVE, ~`0x0073e554..0073e6d3`): `0x7d4` touched at `0073e5e0`, `0073e5f0`,
  `0073e60d` — `breath -= frameTime`; on `breath <= -NPC_DAM_DIVE_TIME` it loses 1 HP and
  rewrites `0x7d4 = 0`.
- regen branch (surfaced/other, ~`0x0073e6d5..0073e713`): `0x7d4` touched at `0073e6d5`,
  `0073e6f0`, `0073e6f6`, `0073e70f`, with `0x7d0` read at `0073e6db/0073e6fc/0073e709` for the
  upper clamp — `breath += 2*frameTime`, clamped to `+0x7d0`.
- sentinel guard reads `+0x7d0` at `0073e535` (breath disabled when `0x7d0 == -1000000`).

`NPC_DAM_DIVE_TIME` (string `@0x008b8a28`) is fetched by the same handler at `0073e57e/0073e58b`,
again reported function-less by `wde find`. So `+0x7d4` is written **continuously while diving**,
falling from `0x7d0` to ~0; the rendered fill `0x7d4/0x7d0` therefore drains from 1.0 to 0 over a
dive — it does NOT stay full.

This directly contradicts `statbar-divebar-never-drains.md`, whose central claim ("across the
entire `.text` … the only writers of `oCNpc+0x7d4` are the constructor, `SetSwimDiveTime`, and
`ResetPos` … nothing ever decrements `0x7d4` … the original bar fill is always 1.0") is an
artifact of scanning only Ghidra-defined functions and missing the `0x0073e480` handler.

## OpenGothic file:line

`game/mainwindow.cpp:231-238`

```cpp
if(showSwimBar) {
  uint32_t gl = pl->guild();
  auto     v  = float(pl->world().script().guildVal().dive_time[gl]);
  if(v>0) {
    auto t = float(pl->diveTime())/1000.f;
    drawBar(p,barMisc,w()/2,h()-10, (v-t)/(v), AlignHCenter | AlignBottom);
    }
  }
```

`Npc::diveTime()` → `MoveAlgo::diveTime()` = `tickCount - diveStart` (ms), `game/game/movealgo.cpp:693`.

## Divergence

Current OpenGothic renders fill `(v-t)/v` = `(dive_time*1000 - breath_elapsed)/(dive_time*1000)`,
draining 1.0 → 0 over the dive and hitting 0 exactly `dive_time` seconds in (first drown damage
follows `NPC_DAM_DIVE_TIME` ms later — matching the original `0x7d4`-crosses-`-tickSz` HP loss).
This **matches** the original bar (`0x7d4/0x7d0`). The drain rate (1× real time), the empty point,
and the drown-onset offset all agree.

The only genuine bar-related divergence that remains is the *gradual refill after surfacing*,
already captured (and DEFERRED) in `dive-breath-regen.md`: the original regenerates `+0x7d4` at
`2×frametime` clamped to `+0x7d0`, whereas OpenGothic re-stamps `diveStart` and refills instantly
on the next dive. That is a separate, deferred item and is not re-litigated here.

The actionable divergence this doc records is the *proposed* one: applying
`statbar-divebar-never-drains.md`'s patch (replace the fill with the literal `1.f`) would make
OpenGothic's bar permanently full and thereby DIVERGE from `Gothic2.exe`, which drains it.

## Proposed patch

**DEFERRED — make no change to `game/mainwindow.cpp:231-238`; the current draining bar is correct
parity.** Do NOT apply the `1.f` patch proposed in `statbar-divebar-never-drains.md`; it is based
on a `0x7d4`-never-written premise that is false (the writers live in the undefined-function
breath handler at `0x0073e480`, sites `0073e5e0/0073e5f0/0073e60d/0073e6d5/0073e6f0/0073e6f6/
0073e70f`, which defined-function scans skip).

Recommended follow-up (doc-only, no build impact): supersede / annotate
`statbar-divebar-never-drains.md` with this correction so the always-full patch is not picked up
later.

Grep-verified OpenGothic symbols referenced: `showSwimBar`/`barMisc`/`MainWindow::drawBar`
(`game/mainwindow.cpp`), `Npc::guild()`, `GameScript::guildVal().dive_time`
(`game/game/gamescript.cpp:411`), `Npc::diveTime()` (`game/world/objects/npc.cpp:1359`),
`MoveAlgo::diveTime()` (`game/game/movealgo.cpp:693`).
