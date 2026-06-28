# Music theme mode-fallback inverts the FGT > THR > STD intensity hierarchy

**Confidence:** High

## Original fn + address

`oCZoneMusic::ProcessZoneList` (Gothic2.exe `0x00640560`) builds the requested
theme name `ZONE_<DAY|NGT>_<STD|THR|FGT>` (suffix appended by
`oCZoneMusic::GetNewTheme` @`0x00641ba0`, status taken from
`oCGame::GetHeroStatus` @`0x006c2d10` -> `oCAIHuman::GetEnemyThreat` @`0x00696950`,
where 0=STD, 1=THR, 2=FGT) and, when the chosen theme is missing, walks a strictly
*descending* intensity fallback before giving up:

- Hero status **FGT (2)**: try `_FGT`; if absent, overwrite the suffix `_FGT -> _THR`
  and retry; if still absent, overwrite the last token with `_STD`. So **FGT -> THR -> STD**.
- Hero status **THR (1)**: the `s_herostatus == 2` block is skipped entirely, so after
  the `_THR` attempt the code drops straight to the `_STD` overwrite. So **THR -> STD**
  (it never escalates up to `_FGT`).
- Hero status **STD (0)**: stays `_STD`.

In other words the original only ever falls *down* the intensity ladder FGT > THR > STD;
a mere threat never borrows the combat (FGT) theme, and a real fight degrades to the
threat theme before the calm theme.

## OG file:line

`game/world/worldsound.cpp:295` (inside `WorldSound::tickSoundZone`):

```cpp
GameMusic::Tags modeTry[] = {mode, mode==GameMusic::Thr ? GameMusic::Fgt : GameMusic::Std, GameMusic::Std};
```

`GameMusic::Tags` (game/gamemusic.h:20): `Std=0, Fgt=1<<1, Thr=1<<2`.

## Divergence

The fallback table expands to:

- `mode==Thr` -> `{Thr, Fgt, Std}`  — **wrong**: a hero merely under *threat* will play the
  *combat/FGT* theme if the zone lacks a `_THR` theme. The original never escalates THR up to FGT.
- `mode==Fgt` -> `{Fgt, Std, Std}`  — **wrong**: a real *fight* skips the `_THR` theme and
  drops straight to calm `_STD` when `_FGT` is missing. The original falls FGT -> THR first.
- `mode==Std` -> `{Std, Std, Std}`  — correct.

So OpenGothic inverted the fallback direction on both the THR and FGT rows: threat can
escalate to combat music, and combat can collapse to calm music, the opposite of the
original's monotone FGT > THR > STD descent.

## Proposed patch

OLD (worldsound.cpp:295):
```cpp
  GameMusic::Tags modeTry[] = {mode, mode==GameMusic::Thr ? GameMusic::Fgt : GameMusic::Std, GameMusic::Std};
```
NEW:
```cpp
  // NOTE: in original-game oCZoneMusic::ProcessZoneList (Gothic2.exe @0x00640560) a missing theme
  // falls back only *down* the intensity ladder FGT>THR>STD: a fight (FGT) degrades to the threat
  // theme then the calm theme (FGT->THR->STD), threat (THR) degrades straight to calm (THR->STD),
  // and threat never escalates up to the combat theme. The previous `mode==Thr ? Fgt : Std`
  // expansion did the reverse (THR borrowed FGT; FGT skipped THR).
  GameMusic::Tags modeTry[] = {mode, mode==GameMusic::Fgt ? GameMusic::Thr : GameMusic::Std, GameMusic::Std};
```

This yields `Fgt -> {Fgt,Thr,Std}`, `Thr -> {Thr,Std,Std}`, `Std -> {Std,Std,Std}`,
matching the original exactly. One-line change; `GameMusic::Thr/Fgt/Std` and the
surrounding loop already exist, so it is build-verifiable.
