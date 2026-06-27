# Mis_GetStatus returns 0 instead of -1 for an unknown / not-running mission

**Confidence:** Medium
(The engine-boundary return-value divergence is verified to high confidence; the
observable gameplay impact is medium because it depends on whether the loaded
Gothic II content scripts actually call the `Mis_*` mission externals.)

## Original function + address

`Mis_GetStatus` is registered as an engine external by `DefineExternals_Ulfi`
(string `u'Mis_GetStatus'` @ `0x008b4a6c`). Its external wrapper is
`FUN_006f8450 @ 0x006f8450` (source tag `oGameExternal.cpp`). The wrapper reads
one int parameter (the mission number), calls
`oCMissionManager::GetStatus @ 0x00715950` with the player NPC and that number,
and then `SetReturn(status)` — i.e. it returns the raw status integer.

`oCMissionManager::GetStatus @ 0x00715950` returns **-1** when the mission cannot
be resolved: it returns -1 immediately if the manager holds no missions
(`count < 1`), and -1 again if the binary-search/look-up for the requested
mission number does not find a live entry. When the lookup misses, the wrapper
additionally logs `Mis_GetStatus() failed for nr: <n>` (string @ `0x008b6494`)
before returning that -1. So the original contract is: **non-existent / not-yet-
offered mission => return -1.** A live mission returns its stored status code
(MIS_RUNNING / MIS_SUCCESS / MIS_FAILED / MIS_OBSOLETE).

The companion externals `Mis_SetStatus` (`u'Mis_SetStatus'` @ `0x008b4a5c`),
`Mis_OnTime` (`u'Mis_OnTime'` @ `0x008b4a50`) and `Mis_AddMissionEntry`
(`u'Mis_AddMissionEntry'` @ `0x008b4a08`) are part of the same
`oCMissionManager` subsystem and are likewise registered by
`DefineExternals_Ulfi`.

## OpenGothic file:line

- `game/game/gamescript.cpp` — `GameScript::initCommon()` binds 204 externals
  (`log_createtopic`/`log_settopicstatus`/`log_addentry` @ lines 307-309,
  `info_*` @ 317-319, etc.). It binds **no** `mis_*` external. Grep for
  `mis_getstatus`/`mis_setstatus`/`mis_ontime`/`mis_addmission`/`missionmanager`
  across `game/` returns nothing — the entire `oCMissionManager` subsystem is
  unimplemented.
- `game/gothic.cpp:964` — every unbound external (including all `Mis_*`) is
  routed to `vm.register_default_external(...)` which only calls
  `Gothic::notImplementedRoutine` (`game/gothic.cpp:1007`, log-once stub).
- `lib/ZenKit/src/DaedalusVm.cc:809-836` — the default-external wrapper pops the
  declared parameters and, when the symbol `has_return()`, pushes a hard-coded
  default: `push_int(0)` for an INT return.

## Divergence

`Mis_GetStatus` has an INT return type, so the unimplemented path pushes **0**.
The original returns **-1** for any mission that is not currently live (which, in
OpenGothic, is *every* mission, since there is no mission manager at all).

Scripts that gate on the documented "not found" sentinel — e.g.
`if (Mis_GetStatus(MIS_X) == -1)` to detect a mission that has not been
offered/started — take the wrong branch in OpenGothic, because the engine answers
0 instead of -1. 0 is not a defined `MIS_*` status code, so any equality check
against the real status constants also behaves inconsistently. This is a genuine
return/sentinel divergence at the external boundary, not merely a missing
feature.

## Proposed patch

**DEFERRED.**

Reason: A correct fix is not surgical. `Mis_GetStatus` cannot be reduced to a
constant — its real return depends on per-mission state maintained by
`oCMissionManager` (the status array, `Mis_SetStatus` transitions, `Mis_OnTime`
timeouts, `Mis_AddMissionEntry` journal coupling). Implementing only
`mis_getstatus` to return `-1` would be a behavior guess: it would be *closer*
to the original for the "never offered" case but would still be wrong the moment
content calls `Mis_SetStatus`/`Mis_OnTime` (those remain no-ops, so a mission set
to MIS_RUNNING would still read back as -1). Per "empty beats false positives,"
shipping a partial stub that masks the missing state machine is worse than the
honest not-implemented log.

A faithful fix requires porting the `oCMissionManager` mission-status state
(create/offer/get/set/on-time) and binding all four `mis_*` externals together,
mirroring `oCMissionManager::GetStatus @ 0x00715950` (return -1 when the mission
number is unknown) and `FUN_006f8450 @ 0x006f8450` (return the stored status,
log `Mis_GetStatus() failed for nr:` on miss). That is a feature port, out of
scope for a single surgical parity patch.

<!-- NOTE: in original-game Mis_GetStatus external FUN_006f8450 @0x006f8450 ->
     oCMissionManager::GetStatus @0x00715950 returns -1 for an unknown / not-live
     mission; OpenGothic has no mission manager, so the default external
     (game/gothic.cpp:964, lib/ZenKit DaedalusVm.cc:822-826) pushes 0 instead. -->
