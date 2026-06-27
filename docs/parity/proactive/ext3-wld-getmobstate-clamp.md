# Wld_GetMobState clamps the "no-state" sentinel (-1) up to 0

**Confidence:** Medium-High

## Original function + address
`Wld_GetMobState` handler, `Gothic2.exe` @ `0x006ed880` (in `oGameExternal.cpp`).
In prose: the handler reads the scheme string and the NPC parameter, then sets the
return value to `-1` as the failure default. If the NPC is non-null it resolves a mob
(the NPC's currently-interacting mob if any, otherwise `FindMobInter(scheme)`); if a mob
is found it returns the mob's raw state via the `GetState` virtual (vtable +0xB4) with
**no clamping**, otherwise it leaves the `-1` default. A mob at rest / never used (or one
rewound after its user was killed) carries state `-1`, and the original surfaces that `-1`
unchanged to the script. (`GetState` is a trivial accessor of the packed state dword at
offset `0x234`; for state `-1` it returns `-1`, never 0.)

## OpenGothic file:line
`/Users/admin/Downloads/opengothic/game/game/gamescript.cpp:1792` (function body), divergent
line `1804`.

## Divergence
```
return std::max(0,mob->stateId());
```
OpenGothic clamps the mob state with `std::max(0, …)`. `Interactive::stateId()` returns the
`state` field, which defaults to `-1` and is explicitly set back to `-1` at runtime for a
mob with no user / after rewind (see `game/world/objects/interactive.cpp:273,287` and the
default `state = -1` at `game/world/objects/interactive.h:178`). For an available-but-unused
mob the original `Wld_GetMobState` returns `-1`, while OpenGothic returns `0` (the clamp maps
`-1 → 0`). Scripts that distinguish "fresh / nobody using it" (`== -1`) from "in first state"
(`== 0`) — e.g. mob-routine and bench/idle checks — observe the wrong value.

## Proposed patch
Grep-verified symbols: `Interactive::stateId()` (`game/world/objects/interactive.h:54`),
`World::availableMob` (returns `Interactive*`).

OLD (`gamescript.cpp:1804`):
```cpp
  return std::max(0,mob->stateId());
```
NEW:
```cpp
  // NOTE: in original-game Wld_GetMobState @0x006ed880 the resolved mob's raw state (oCMobInter::
  // GetState, vtable +0xB4) is returned unclamped; an available-but-unused/rewound mob has state
  // -1 and the original surfaces -1, not 0. Only the npc-null / no-mob-found paths yield -1.
  return mob->stateId();
```
The existing `-1` returns for the null-NPC and null-mob paths (lines 1796, 1801) already match
the original's failure default, so only the spurious `std::max(0, …)` clamp needs removal.

## Secondary observation (NOT patched)
The original resolves the mob as "currently-interacting mob first, else `FindMobInter(scheme)`",
whereas OpenGothic always uses `world().availableMob(*npc, scheme)`. This is a separate, riskier
selection-semantics difference and is left DEFERRED (not a surgical clamp/sentinel fix).

## Externals checked this sweep
- **Wld_GetMobState** — FINDING above (unclamped `-1` vs OG `max(0,…)`).
- **Npc_GetTrueGuild** @0x006ee580 — FAITHFUL: original returns `0` on null NPC; OG returns
  `GIL_NONE`, and `GIL_NONE == 0` (`game/game/constants.h:9`). Non-null path matches.
- **Npc_GetDistToItem** @0x006f2fb0 — FAITHFUL: original returns `0x7fffffff` when either vob is
  null and otherwise `__ftol(GetDistanceToVob)` with no overflow clamp; OG returns `INT32_MAX` on
  null and `int32_t(dp.length())` otherwise. Magnitude and sentinel match.
- **Wld_IsTime** (`gamescript.cpp:1742`) — FAITHFUL: zero-width window already handled
  (NOTE @0x00781190 present).
- **Npc_GetBodyState** (`gamescript.cpp:2283`) — already noted FAITHFUL in prior sweep.
- **Npc_GetPermAttitude** (`gamescript.cpp:2772`) — already fixed (NOTE @0x0072fb30).
- **PrintDebug family / Snd_IsSourceNpc / Snd_GetDistToSource** — not bound in OpenGothic
  (`bindExternal` list lines 111-326); no handler to diverge.
