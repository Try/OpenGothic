# Combat/threat music re-evaluation latency (THR/FGT engage + back-to-STD)

**Confidence:** Medium-High (divergence is High; exact replacement interval is the soft part)

## Original fn + address
The hero music status drives THR/FGT/STD selection through
`oCZoneMusic::ProcessZoneList` (Gothic2.exe @0x00640560), which calls
`oCGame::GetHeroStatus` @0x006c2d10 → `oCAIHuman::GetEnemyThreat` @0x00696950
*every time it runs*, and re-themes immediately whenever the returned status
changes (`s_herostatus != oVar3` branch). `ProcessZoneList` is invoked through
the music vob's virtual update (vtable slots at .data 0x0083adfc / 0x0083ae8c),
i.e. per world/music tick — there is no multi-second gate above it.

`GetEnemyThreat` itself contains what looks like a self-throttle, but it is inert
in the shipped binary. At the prologue (0x0069698a..0x006969ae):
`DAT_00aad6a4 += DAT_0099b3d8; fcomp [0x82e8ac]` and recompute when
`DAT_00aad6a4 >= [0x82e8ac]`. The threshold constant at 0x82e8ac reads **0.0**,
and the only "reset" of the accumulator is `mov eax,[0xaad6a4] ; mov [0xaad6a4],eax`
at 0x006969b9/0x006969bf — a load-then-store-back **no-op** (verified by
disassembly). So once warmed up the accumulator stays `>= 0` and the full
nearby-attacker scan runs on **every** call. Net effect: the original
re-evaluates the hero combat-music status essentially every frame, so THR/FGT
engages and STD resumes with near-zero latency / no hysteresis.

## OG file:line
`game/world/worldsound.cpp:241-243` (`WorldSound::tickSoundZone`):
```
if(owner.tickCount()<nextSoundUpdate)
  return;
nextSoundUpdate = owner.tickCount()+5*1000;
```
This 5-second gate wraps the *entire* combat-music decision (the
`isTargeted(player)` / `isFgt` / Thr-vs-Fgt block at lines ~263-285 and the
`setMusic` fallback). It gates only `tickSoundZone`; the ambient `worldEff` loop
in `WorldSound::tick()` is unaffected.

## Divergence
OpenGothic only recomputes the threat/fight music state once every 5000 ms,
whereas the original recomputes it continuously. Consequence: when an enemy
starts attacking (or the hero draws a weapon) the switch to THR/FGT can lag by
up to ~5 s, and — the symmetric back-to-normal case the brief calls out — after
the last attacker disengages the music can keep playing THR/FGT for up to ~5 s
before STD resumes. The original has no such cool-down; the cached status
(`DAT_00aad6a8`) is reset to 0 at the top of each recompute and only the
sticky WALD/near-death THR survives, so STD returns on the very next frame with
no attacker present.

## Proposed patch
Decouple the combat-music mode evaluation from the 5 s ambient-zone throttle so
it tracks the original's per-tick responsiveness. Minimal surgical form — shrink
the gate that wraps the mode decision:

OLD (`game/world/worldsound.cpp:241-243`):
```cpp
  if(owner.tickCount()<nextSoundUpdate)
    return;
  nextSoundUpdate = owner.tickCount()+5*1000;
```
NEW:
```cpp
  // NOTE: in original-game oCZoneMusic::ProcessZoneList @0x00640560 the hero
  // music status (oCGame::GetHeroStatus @0x006c2d10 -> oCAIHuman::GetEnemyThreat
  // @0x00696950) is recomputed every music tick and the theme is re-selected
  // immediately on any STD/THR/FGT change: GetEnemyThreat's internal throttle is
  // inert in the retail binary (threshold @0x82e8ac == 0.0 and the accumulator
  // reset at 0x006969bf is a load/store no-op), so combat music has no engage or
  // back-to-STD latency. The 5 s gate here delayed THR/FGT entry and the return
  // to STD by up to ~5 s. Re-evaluate combat-music mode each tick instead.
  if(owner.tickCount()<nextSoundUpdate)
    return;
  nextSoundUpdate = owner.tickCount()+200; // was 5*1000
```

DEFERRED note on going fully per-frame: the faithful 1:1 is to drop the gate
entirely, but `WorldSound::tickSoundZone` runs `WorldObjects::isTargeted`
(`game/world/worldobjects.cpp:435`, a `Workers::parallelFor` over every NPC)
each pass. A small interval (e.g. 200 ms) preserves the perceptual
"immediate" feel while bounding that scan's cost; removing the gate outright is
a larger change that should be perf-validated separately. Build-verifiable
either way (one-line constant change, no new symbols).
