# PrintScreen display time is 1000 ms too long (and wraps on -1)

Confidence: High

## Original function + address
Script external `PrintScreen` lives in `oGameExternal.cpp` (Ghidra
`FUN_006e2da0 @ 0x006e2da0`, registered by `DefineExternals_Ulfi`). It pulls
the parameters time / font / posy / posx, then computes the display duration as
simply `time_seconds * 1000` and hands that millisecond value to
`zCView::PrintTimed / PrintTimedCX / CY / CXY`. The NPC-event twin
`oCNpc::EV_PrintScreen @ 0x00759...` does the identical `time * 1000`.

`zCView::CreateText` then stores that millisecond value verbatim (field at
offset 0x24 of the `zCViewText`) and the view tick decrements it. There is no
constant added anywhere. The per-character length default
(`strlen * _DAT_008bc868`) only fires inside `PrintTimed` when the float time is
exactly `-1.0`; because the script path already multiplied by 1000 the value is
`-1000.0`, so for an explicit `-1` the text simply elapses on the first tick
(negative remaining time). Net: original on-screen duration == `time*1000` ms,
no fudge, and `-1` means "gone immediately", not "forever".

## OG file:line
`game/ui/dialogmenu.cpp:292`

```
e.time = uint32_t(time*1000)+1000;
```

## Divergence
- For any `time >= 0` the message lingers a full extra second versus the
  original (e.g. a scripted `PrintScreen(...,3)` shows 4000 ms instead of 3000).
  This affects every on-screen print (all paths funnel through
  `DialogMenu::printScreen`, the sole sink of `Gothic::onPrintScreen`).
- For `time == -1` the original expires immediately, but here
  `uint32_t(-1000)+1000` wraps to a ~4.29e9 ms value (text persists for days).

## Proposed patch
File: `game/ui/dialogmenu.cpp`

OLD:
```cpp
  e.time = uint32_t(time*1000)+1000;
```
NEW:
```cpp
  // NOTE: in original-game (oGameExternal.cpp PrintScreen / oCNpc::EV_PrintScreen)
  // the on-screen lifetime is exactly time*1000 ms with no constant added; a
  // negative time elapses on the first tick rather than persisting.
  e.time = time>0 ? uint64_t(time)*1000u : 0u;
```
