# Name-uppercase sweep: waypoint / freepoint / vob / mob / trigger lookups — NO FINDING

**Confidence:** High (negative result)

## Scope

Swept every OpenGothic Daedalus external that takes a WAYPOINT / FREEPOINT / VOB /
SPOT / MOB / TRIGGER-target NAME string and forwards it to a case-sensitive lookup,
to find any case where the original `Gothic2.exe` applies `zSTRING::Upper` but
OpenGothic does not. The 5 already-fixed externals (ai_gotofp, ai_gotonextfp,
wld_isfpavailable, wld_isnextfpavailable, wld_ismobavailable) were excluded.

## Method (authoritative: callers of zSTRING::Upper)

Enumerated every caller of `zSTRING::Upper` (`Gothic2.exe @0x0046ab00`) and isolated
the ones inside the oGameExternal handler range. The COMPLETE set of external
handlers that upper-case a name argument is:

- `FUN_006eb5b0` Wld_IsFPAvailable, `FUN_006eb860` Wld_IsNextFPAvailable,
  `FUN_006ebfa0` AI_GotoFP, `FUN_006ec270` AI_GotoNextFP — the 4 FP externals (already fixed).
- `FUN_006f5e20` Wld_IsMobAvailable (already fixed).
- `FUN_006eebe0` Hlp_StrCmp — uppers BOTH operands (case-insensitive compare);
  **already fixed** in `game/gothic.cpp:1041` (`Gothic::hlp_strcmp`, NOTE @0x6eebe0).
- `FUN_006dd510` Npc_ExchangeRoutine — see below, faithful.
- `FUN_006fa4f0` Mdl_ApplyRandomAni, `FUN_006fa8c0` Mdl_ApplyRandomAniFreq —
  upper-case an ANIMATION name, not a point/vob/mob lookup (out of scope).

## Per-candidate result (all faithful)

- **AI_GotoWP** (`FUN_006eaf50`): no `zSTRING::Upper`. Lookup is `zCWayNet::GetWaypoint`
  (`@0x007b0330`), a byte-exact case-sensitive compare. OG `ai_gotowp` likewise does not
  upper and uses case-sensitive `findWayPoint`. Faithful.
- **Npc_GetDistToWP** (`FUN_006f2c30`): no Upper; same `GetWaypoint` path. OG faithful.
- **Wld_InsertNpc** (`FUN_006df1f0`) / **Wld_InsertItem** (`FUN_006e0520`): no Upper on
  the spawnpoint. OG faithful.
- **Wld_GetMobState** (`FUN_006ed880`): no Upper — passes the raw param to
  `oCNpc::FindMobInter` (`@0x0073fe70`, `zSTRING::Search`). This is a genuine vanilla
  asymmetry vs Wld_IsMobAvailable (which DOES upper). OG `wld_getmobstate` correctly does
  NOT upper → faithful.
- **Wld_SetMobRoutine** (`FUN_006def50`): no Upper (vob-by-name lookup). OG faithful.
- **Wld_SendTrigger / Wld_SendUntrigger**: not in the Upper-caller set. OG faithful.
- **Mob_HasItems** (`FUN_006f6c70`): no Upper. OG faithful.
- **Npc_ExchangeRoutine** (`FUN_006dd510`): original uppers the routine name, but OG builds
  `"Rtn_<name>_<id>"` and resolves via `DaedalusScript::find_symbol_by_name`
  (`lib/ZenKit/src/DaedalusScript.cc:133/159`), which itself upper-cases the query
  (`std::transform(..., toupper)`) → case-insensitive. Equivalent behavior. Faithful.

## Conclusion

NO FINDING. Every in-scope name->object lookup external that the original upper-cases is
already fixed; the remaining waypoint/vob/mob/trigger externals do not upper-case in the
original either, and OpenGothic matches that. No surgical fix warranted.
