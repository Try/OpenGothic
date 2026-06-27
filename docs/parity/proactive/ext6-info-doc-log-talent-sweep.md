# ext6 — Info_/Doc_/Log_/talent dialog-externals sweep

**Result: NO FINDING** (no new surgical wrong-default / missing-effect / wrong-arg divergence)

**Confidence:** High that the externals below are faithful; the only residual gaps are
pre-existing, explicitly-marked `// TODO` incompleteness, not surgical parity bugs.

## Scope
Fresh decompile-vs-OG comparison of the dialog-related Daedalus externals in
`game/game/gamescript.cpp` (and `game/gothic.cpp` for Doc_*), checking the EFFECT against
the original `Gothic2.exe` handlers.

## Externals verified FAITHFUL

- **Info_AddChoice** — original handler `FUN_006dbe20`; the choice is inserted at the
  HEAD of the list via `oCInfo::AddChoice` @0x00703b20 (`head = new; new->next = oldHead`),
  i.e. a PREPEND. zenkit `IInfo::add_choice` (`lib/ZenKit/src/addon/daedalus.cc:300`,
  `choices.insert(choices.begin(), ch)`) also prepends, and `updateDialog`
  (`gamescript.cpp:945`) iterates `choices` index 0..n — matching the original's head-first
  collection order. Argument order (info, text, func) also matches the parser pop order.
- **Info_ClearChoices** — original `FUN_006dc1a0` → `oCInfo::RemoveAllChoices` @0x00703d70
  clears the whole list; OG `info->choices.clear()` (`gamescript.cpp:3488`) matches.
- **Log_CreateTopic** — section gate `{Mission=0, Note=1}` (`gamescript.cpp:3388`,
  `questlog.h:20`) matches the original LOG_MISSION/LOG_NOTE constants.
- **Log_SetTopicStatus** — already covered (`log-settopicstatus-create.md`,
  phantom-quest); status gate {Running..Obsolete} faithful.
- **TA_MIN** — original `FUN_006dcca0` pops (wp, action, stop_m, stop_h, start_m, start_h)
  and calls `InsertRoutine(start_h,start_m,stop_h,stop_m,action,wp,-1)`; OG `ta_min`
  (`gamescript.cpp:3382`) → `addRoutine(gtime(start_h,start_m),gtime(stop_h,stop_m),action,wp)`
  — argument order and waypoint forwarding match.
- **Npc_GetTalentSkill** — original `oCNpc::GetTalentSkill` @0x007317f0 valid range and
  return value match OG `Npc::talentSkill` (`npc.cpp:1195`); the only difference is the
  original returns -1 for the unused overrun index 22 where OG returns 0 (TALENT_MAX_G2=22,
  no real talent there) — functionally inert.
- **Npc_SetTalentSkill** — external `FUN_006e68e0` applies NO cap to the level before
  `oCNpc::SetTalentSkill` @0x00730f60; OG `Npc::setTalentSkill` (`npc.cpp:1188`) also stores
  the raw value with no cap. Faithful. (The original additionally re-applies fight/overlay
  MDS and music inside SetTalentSkill; OG handles overlays separately via
  `invalidateTalentOverlays` — out of scope for this sweep.)
- **Npc_GetStateTime** — original `oCNpc_States::GetStateTime` @0x0076c0a0 returns
  elapsed/1000; OG (`gamescript.cpp:2359`) returns `stateTime()/1000`. Faithful (original
  additionally returns 0 when its float accumulator is exactly 0, an inert edge case).
- **Doc_SetPage** — original `FUN_006d9c80` pops (scale, img, page, doc) and calls
  `oCDocumentManager::SetPage(doc,page,img)` IGNORING scale; OG `doc_setpage`
  (`gothic.cpp:1101`, `//TODO: scale`) likewise ignores scale. Page<0 → document-level
  background matches. Faithful.
- **Doc_PrintLine / Doc_PrintLines** — both append text + "\n" in OG; the original
  difference (`zCViewPrint::PrintLine` no-wrap vs `PrintLines` word-wrap) is a renderer-level
  concern that OG's DocumentMenu re-wraps anyway. No external-boundary divergence.
- **Wld_StopEffect** — OG `wld_stopeffect` (`gamescript.cpp:1694`) resolves the VFX and
  calls `World::stopEffect` (name-matched removal), consistent with the original.

## Residual gaps (NOT surgical — pre-existing acknowledged TODO)

- **Wld_PlayEffect targeting** — original `FUN_006dfc20` calls
  `oCVisualFX::CreateAndPlay(name, origin, target, level, damage, dmgType, isProjectile)`
  for ANY zCVob origin/target pair. OG `wld_playeffect` (`gamescript.cpp:1662`) early-returns
  with `"effect not implemented"` whenever `effectLevel|damage|damageType|isProjectile != 0`,
  and only implements the npc→npc and item→item combos (npc→item / item→npc do nothing).
  This is the existing `// TODO` partial implementation, not a one-line wrong-default; a
  faithful fix requires the unimplemented damage/level/projectile effect path and is out of
  scope for a surgical parity patch. DEFERRED.

## Could not verify
- **Doc_SetMargins `mul`** — OG multiplies each margin by `mul` (`gothic.cpp:1140`). The
  original external handler is an unlisted function (~0x6d9db0, between Doc_SetPage and
  Doc_SetFont) absent from the Ghidra function DB; with the MCP server offline it could not
  be decompiled. Script convention (`mul` consistently passed as 1) makes OG's identity
  behavior the most likely faithful one, so no patch is proposed (empty beats false
  positives).
