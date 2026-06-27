# Dive / breath bar fill: original renders it permanently FULL, OpenGothic drains it

**Confidence:** High (on the divergence; backed by a complete Gothic2.exe field-access scan).
The desirability of the fix is a maintainer judgement call — see "Nuance" below.

## Original function + address (prose)

`oCGame::UpdatePlayerStatus` @ `0x006c3140` (`oGame.cpp`) builds the four HUD status
bars (HP @ `this+0x8c`, dive bar @ `this+0x90`, mana @ `this+0x94`, focus @ `this+0x98`).
The four `oCViewStatusBar` virtuals are, by vtable offset: `0x30 = SetMaxRange(min,max)`,
`0x34 = SetRange(min,max)`, `0x38 = SetPreview(v)`, `0x3c = SetValue(v)`; the rendered
fill is `(value - usedMin) / (usedMax - usedMin)` (verified in `zCViewStatusBar::SetValue`
@ `0x0046f860` / `SetRange` @ `0x0046f770`).

The dive bar (slot `0x90`, the `BAR_MISC` bar) is inserted only while the player's
`oCAniCtrl_Human` walk-mode (`anictrl+0x160`) equals `5` (`WM_DIVE`). Its values come from
`oCNpc::GetSwimDiveTime` (helper @ `0x00741fa0`), which returns three raw `oCNpc` fields:
`0x7cc` (swim-time), `0x7d0` (dive-time max), `0x7d4` (dive "remaining"). The disassembly
at `0x006c3378..0x006c33d3` does:

- `SetMaxRange(0, (int)npc[0x7d0])` and `SetRange(0, (int)npc[0x7d0])`  → range max = dive-time max
- `SetValue(npc[0x7d4])`  → value = dive "remaining"
  (an `fcomp` guard at `0x006c3385` overwrites the value with `0` only when the max equals the
  sentinel constant at `0x0083c280`, i.e. the max-is-zero case.)

So the rendered fill is `npc[0x7d4] / npc[0x7d0]`.

**Key fact (complete binary scan):** across the entire `.text` of `Gothic2.exe`, the only
writers of `oCNpc+0x7d4` are the constructor (`=0`), `oCNpc::SetSwimDiveTime` @ `0x00741f70`
(`0x7d4 = dive_time*1000`, set together with `0x7d0`), and `oCNpc::ResetPos` @ `0x006824d0`
(`0x7d4 = 0x7d0`). **Nothing ever decrements `0x7d4`.** `oCNpc::CanDive`/`CanSwim` only test
the static maxima `0x7d0`/`0x7cc` for `>0`; the drowning-damage timer is a separate mechanism.
Therefore `npc[0x7d4]` is *always equal to* `npc[0x7d0]`, and the original dive bar fill is
**always 1.0** — it shows full for the whole dive and never drains.

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

## Divergence

OpenGothic computes a *draining* fill `(v - t)/v`, where `v = dive_time` and
`t = diveTime()/1000` is the elapsed time under water (`Npc::diveTime()`
= `tickCount - diveStart`, `game/game/movealgo.cpp:688`). The OG bar therefore empties from
1.0 to 0.0 over the dive and reaches 0 exactly when drown damage begins
(`Npc::tick` drown branch, `game/world/objects/npc.cpp:2432-2447`).

The original bar does **not** drain — it is rendered permanently full while diving (value
field `0x7d4` is never decremented). This is a behavioral divergence in the dive/breath bar
*fill*, within the status-bar value-computation subsystem, and distinct from the already-fixed
focus-HP dead-gate and `showManaBar` gating.

## Proposed patch

```cpp
// OLD (game/mainwindow.cpp:231-238)
if(showSwimBar) {
  uint32_t gl = pl->guild();
  auto     v  = float(pl->world().script().guildVal().dive_time[gl]);
  if(v>0) {
    auto t = float(pl->diveTime())/1000.f;
    drawBar(p,barMisc,w()/2,h()-10, (v-t)/(v), AlignHCenter | AlignBottom);
    }
  }

// NEW
if(showSwimBar) {
  uint32_t gl = pl->guild();
  auto     v  = float(pl->world().script().guildVal().dive_time[gl]);
  if(v>0) {
    // NOTE: in original-game oCGame::UpdatePlayerStatus @0x006c3140 the dive bar value is the
    // oCNpc "dive remaining" field (npc+0x7d4) over its range max (npc+0x7d0). A full .text scan
    // of Gothic2.exe shows 0x7d4 is only ever set EQUAL to 0x7d0 (oCNpc::SetSwimDiveTime
    // @0x00741f70, oCNpc::ResetPos @0x006824d0) and is never decremented, so the original bar
    // renders permanently full while diving (it does not drain).
    drawBar(p,barMisc,w()/2,h()-10, 1.f, AlignHCenter | AlignBottom);
    }
  }
```

All cited OG symbols are grep-verified to exist: `showSwimBar`, `barMisc`, `Npc::guild()`,
`GameScript::guildVal().dive_time`, `MainWindow::drawBar` (`game/mainwindow.cpp:679`),
`Npc::diveTime()` (`game/world/objects/npc.cpp:1359`).

## Nuance (read before applying)

OpenGothic's draining bar is arguably the *more useful* behavior (it visually previews the
onset of drowning damage), and may have been a deliberate embellishment rather than an
oversight. The patch above restores strict parity with `Gothic2.exe` (always-full bar) and is
surgical/build-verifiable, but it is a visual downgrade. If the project prefers to keep the
informative draining bar, treat this as **DOCUMENTED-DIVERGENCE** rather than applying the
patch. The divergence itself is established with high confidence.
