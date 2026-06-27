# TA overlay routines: relative-time insertion + removable-overlay stack are not implemented

**Confidence:** Medium-High (divergence is definite and well-localised in the original; practical impact is bounded by how often vanilla G2 scripts wrap TA in `TA_BeginOverlay`/`TA_EndOverlay`).

## Original function + address (prose only)

- `TA_BeginOverlay` external (`Gothic2.exe` ~0x006dd270) calls `oCNpc_States::BeginInsertOverlayRoutine` (0x0076e250). That method: if an overlay stack is already open it tears it down (resets the overlay count at this+0xb0, `oCRtnManager::RemoveOverlay`, re-runs `InitRoutine`, `UpdateSingleRoutine`); it then calls `oCRtnManager::RemoveRoutine` to drop the *current* routine entries, zeroes the count/active/last pointers (this+0xb0, this+0xa4, this+0xa0) and sets the "overlay-insertion active" flag this+0xac = 1.

- `TA_Min` external (0x006dcca0) calls `oCNpc_States::InsertRoutine` (0x0076e4a0). The decisive branch: **when the overlay-active flag (this+0xac) is non-zero**, the routine's start/stop are NOT used as absolute clock times. The method reads the current world time via `oCWorldTimer::GetTime`, then `oCWorldTimer::AddTime(now, start_h, start_m)` and `AddTime(now, stop_h, stop_m)`, i.e. the entry window becomes `[now+start, now+stop]` (relative offsets), and the overlay count this+0xb0 is incremented. When the flag is zero (ordinary daily TA), the raw h/m are passed straight through (absolute). The newly built `oCRtnEntry` records the overlay flag in its +0x38 field (`= this+0xac`).

- `TA_EndOverlay` external (0x006dd350) calls 0x0076e2c0: it appends a closing/"return" `oCRtnEntry` cloned from the still-active routine (entry at this+0xa0: a zero-length window `[t,t]` at that routine's waypoint, overlay flag from this+0xac), then `CreateWayBoxes` + `UpdateSingleRoutine`, and clears this+0xac = 0.

- `TA_RemoveOverlay` external (0x006dd430) calls `oCNpc_States::RemoveOverlay` (0x0076e460): clears the overlay count, `oCRtnManager::RemoveOverlay`, `InitRoutine`, `UpdateSingleRoutine` — i.e. pops the overlay stack and restores the base daily routine.

- Tie-break consequence: `oCRtnManager::Sort_Routine` (0x00774600), used by `oCRtnManager::Insert`'s InsertSort, compares `start_h`, then `start_m`, and on equal start times places **overlay entries (entry+0x38 != 0) before non-overlay entries**. So an overlay slot beginning at the same minute as a daily slot wins in `FindRoutine` (0x00775580).

## OpenGothic file:line

- `game/game/gamescript.cpp:305` — only `ta_min` is bound; there is **no** `bindExternal("ta_beginoverlay"/"ta_endoverlay"/"ta_removeoverlay", …)`.
- `game/gothic.cpp:964` — unbound externals fall through to `register_default_external(... notImplementedRoutine ...)`, i.e. they are silent no-ops that just pop their arguments.
- `game/game/gamescript.cpp:3455` `GameScript::ta_min` → `game/world/objects/npc.cpp:4618` `Npc::addRoutine` — always stores `gtime(start_h,start_m)`/`gtime(stop_h,stop_m)` as **absolute** day-times; there is no overlay-active flag, no relative-time path, and `Npc::Routine` has no overlay marker.

## Divergence

For any NPC whose routine function (or a later script) does
`TA_BeginOverlay(self); TA_<x>(self, h0,m0, h1,m1, "WP"); TA_EndOverlay(self);`:

1. **Relative vs absolute window.** Original: the slot is active over `[now+h0:m0 , now+h1:m1]` (a window measured from the moment the overlay was opened — the idiom for "do X for the next N hours starting now"). OpenGothic: the `ta_min` inside the block runs through the default-external no-op for begin/end and is added by `addRoutine` as a permanent **absolute** daily slot `[h0:m0 , h1:m1]`. With the common form `TA_...(self, 0,0, N,0, …)` the original schedules "now .. now+N h", whereas OpenGothic schedules "00:00 .. 0N:00 every day", a different (and persistent) result.

2. **Stacking / removal.** `TA_BeginOverlay` in the original first drops the existing routine entries and remembers a base, and `TA_RemoveOverlay` pops back to the base daily routine. In OpenGothic begin/end/remove do nothing, so overlay TA slots are simply **accumulated** on top of the daily routine and can never be removed by `TA_RemoveOverlay`; the daily routine is also not torn down/rebuilt as the original does.

3. **Priority.** Original tie-breaks equal-start overlay slots ahead of daily slots (`Sort_Routine`); OpenGothic's `addRoutine` `std::stable_sort` keys on `start` only and has no overlay concept, so an overlay slot starting at the same minute as a daily slot does not take precedence.

## Proposed patch

**DEFERRED.** A faithful fix is not surgical: it requires (a) binding three new externals (`ta_beginoverlay`/`ta_endoverlay`/`ta_removeoverlay`) in `GameScript`, (b) adding overlay state to `Npc` (an "overlay-insertion active" flag, an overlay-entry marker on `Npc::Routine`, and a saved base/active pointer), (c) a relative-time path in `Npc::addRoutine` that offsets start/stop by the current `owner.time()` when the overlay flag is set, (d) the `TA_EndOverlay` closing-entry synthesis, and (e) overlay-aware tie-breaking + a `TA_RemoveOverlay` pop. This touches save/load (`Npc::Routine` serialization at `game/world/objects/npc.cpp:347`/`374`) and `currentRoutine`/`endTime` semantics, so it should be designed as a unit rather than patched piecemeal. Recommend implementing behind the existing routine code with explicit `// NOTE: in original-game oCNpc_States::InsertRoutine @0x0076e4a0 / BeginInsertOverlayRoutine @0x0076e250 / Sort_Routine @0x00774600 …` citations.

Lower-impact partial step (still correct as far as it goes): bind the three externals so `TA_BeginOverlay`/`TA_EndOverlay` at minimum gate which TA slots replace the daily routine, even before the relative-time math is added.
