# bind4: Snd_GetDistToSource (and Npc_GetComrades) — closest unbound externals

**Confidence:** DEFERRED (no high-confidence trivial fix found this pass)

## Summary
Of the prompt's candidate list, only two are *real* Gothic2.exe engine externals that
are genuinely UNBOUND in OpenGothic: `Snd_GetDistToSource` and `Npc_GetComrades`. Both
were decompiled and both depend on per-NPC engine state that OpenGothic does not track,
so neither is a 1-4 line surgical fix with existing building blocks. The remaining
prompt candidates are either already bound or are not engine externals at all.

## Bound-list verification
Authoritative bind list built from all 236 `bindExternal(`/`register_external(` sites in
`game/game/gamescript.cpp` (lines 111-330) and `game/gothic.cpp` (lines 971-1004).

Per-candidate status (grep count of its `bindExternal("...")` / engine-string presence):

| Candidate | OG bound? | Engine string in Gothic2.exe? |
|---|---|---|
| Npc_GetHeightToItem | BOUND (gamescript.cpp:249) | yes |
| Npc_GetActiveSpellIsScroll | BOUND (gamescript.cpp:207) | yes |
| Npc_GetActiveSpellLevel | BOUND (gamescript.cpp:210) | yes |
| Npc_GetReadiedWeapon | BOUND (gamescript.cpp:221) | yes |
| Npc_GetDetectedMob | BOUND (gamescript.cpp:242) | yes |
| Hlp_IsValidItem | BOUND (gamescript.cpp:113) | yes |
| Hlp_IsItem | BOUND (gamescript.cpp:114) | yes |
| Mob_HasItems | BOUND (gamescript.cpp:305) | yes |
| Wld_DetectItem (`wld_detectitem`) | BOUND (gamescript.cpp:140) | yes |
| Npc_HasFightTalent | UNBOUND | **no engine string** — not a Gothic2 engine external |
| Wld_DetectItemEx | UNBOUND | **no engine string** — not a Gothic2 engine external |
| Doc_GetFont | UNBOUND | **no engine string** — not a Gothic2 engine external |
| **Snd_GetDistToSource** | **UNBOUND** | yes (`008b4ab4`) |
| **Npc_GetComrades** | **UNBOUND** | yes (`008b5500`) |

`Npc_HasFightTalent`, `Wld_DetectItemEx`, `Doc_GetFont` have no name string in
Gothic2.exe (`wde strings` returns none) and no `DefineExternals_Ulfi` registration, so
they are NOT engine externals — binding them would be a false positive. Excluded.

## Candidate 1 — Snd_GetDistToSource (closest)
**Original fn + address (prose):** the external handler is at `FUN_006f91c0`
(registered via `DefineExternals_Ulfi`, name string `s_Snd_GetDistToSource` @ `0x008b4ab4`).
It pops one NPC argument and returns `(int)oCNpc::GetDistToSound(npc)` (the float result is
truncated via `__ftol`). `oCNpc::GetDistToSound` @ `0x0075d500` computes an *octagonal
approximation* of the Euclidean distance between the NPC's world position (object fields at
`+0x48/+0x58/+0x68`) and a stored **sound-source position** held on the NPC at
`+0x9f4/+0x9f8/+0x9fc`. That source position is written by the engine when a sound/noise
perception (PERC_ASSESSQUIETSOUND family) is delivered to the NPC.

**OG file:line (where it would bind / where state would live):**
`game/game/gamescript.cpp:324-325` (Snd_* bind block) for the binding;
`game/world/objects/npc.{h,cpp}` for the missing state;
`game/world/worldobjects.cpp:959-976` (`sendPassivePerc` path) and
`game/world/objects/npc.cpp:2467` (`PERC_ASSESSQUIETSOUND` emission) for where the
source position is currently *available but not persisted*.

**Divergence:** OpenGothic never stores a per-NPC "last sound-source position". The
perception path has `msg.pos` (`worldobjects.cpp:959`) and `perceptionProcess` receives only
`quadDist` (`npc.h:299`) — neither is retained on the NPC after the perception is handled.
So there is no live position to compute distance from when the script later calls
`Snd_GetDistToSource(self)` inside `B_AssessQuietSound`.

**Why DEFERRED (not a surgical fix):** a faithful implementation requires
(a) a new `Tempest::Vec3` member on `Npc` for the sound-source position,
(b) threading `msg.pos` through `sendPassivePerc`/`perceptionProcess` (signature change
across `worldobjects.cpp` + `npc.{h,cpp}`) to set it on PERC_ASSESSQUIETSOUND,
(c) save/load serialization of the new field for save-state parity, and
(d) the binding itself.
The original recomputes distance *live* against the stored source position each call (so it
tracks NPC movement after the perception), meaning a shortcut of caching the
distance-at-perception-time would diverge. This is multi-file and touches the save format —
outside the "1-4 line, building blocks exist" bar. **No patch proposed.**

## Candidate 2 — Npc_GetComrades (rejected: non-trivial)
Handler `FUN_006f39b0` (name string `0x008b5500`) returns `oCNpc::GetComrades(npc)`
@ `0x007410b0`. That method does a spatial `CollectVobsInBBox3D` query around the NPC,
filters by class, checks each candidate's state (`IsInState`), active spell list (transform
spells 0x21/0x47, plus `IsSpellActive` 0x52/0x27/0x25/0x56), and same-guild ownership, then
counts qualifying NPCs. This needs a bbox vob query plus active-spell/guild filtering — well
beyond a trivial reimplementation and reliant on subsystems not wired for this purpose.
Rejected.

## Conclusion
No candidate this pass satisfies *unbound AND trivially implementable (1-4 lines) AND
building blocks already present*. The trivially-expressible candidates are already bound;
the two genuinely-unbound real externals each need new persisted NPC state or a spatial
query. Per "empty beats false positives," no edit is made. `Snd_GetDistToSource` is the
recommended next target once a per-NPC sound-source position is added to the perception path
(it would then be a one-line bind plus a `qDistTo`/`sqrt` accessor).
