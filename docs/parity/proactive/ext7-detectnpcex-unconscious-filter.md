# Wld_DetectNpcEx detects unconscious NPCs that the original filters out

**Confidence:** Medium-High

## Original function + address

The external `Wld_DetectNpcEx` (handler `FUN_006e15c0` in the `_ulf`
oGameExternal layer) pops its five parameters and, on success, calls
`oCNpc::FindNpcEx` (Gothic2.exe @ `0x00740b80`) with arguments
`FindNpcEx(npcInstance, guild, aiState, /*nearest*/1, /*excludePlayer*/(detectPlayer==0), /*aliveOnly*/1)`.

Inside `FindNpcEx`, the trailing `aliveOnly` flag (passed as `1`) gates an
accepted candidate through a liveness test before it is considered: the
candidate is only eligible when its hitpoints are `> 0` **and** it is *not* in
the special unconscious state (`IsInState(-4)`) **and** *not* in the dead state
(`IsInState(-5)`) **and** not currently under a transform/control magic effect.
In other words the original requires the target to be alive *and conscious*
(and untransformed). For comparison, the plain `Wld_DetectNpc` handler
(`FUN_006e11d0` → `oCNpc::FindNpc` @ `0x00740a80`) passes neither flag and
applies no liveness filter at all.

The `(detectPlayer==0)` → exclude-player mapping and the `aiState`/`guild`/
`instance` filters in the OpenGothic port are faithful; only the liveness gate
diverges.

## OpenGothic file:line

`game/game/gamescript.cpp:1849` — `GameScript::wld_detectnpcex`, the predicate
inside the `world().detectNpc(...)` lambda.

## Divergence

OpenGothic filters the candidate with `!n.isDead()` only
(`GameScript::isDead` → `isInState(ZS_Dead)`, the analogue of the original's
`IsInState(-5)`). It never excludes the *unconscious* state
(`isInState(ZS_Unconscious)` ≙ original `IsInState(-4)`). Consequently an NPC
that has been knocked out (HP intact, but in `ZS_Unconscious`) is reported by
OpenGothic's `Wld_DetectNpcEx`, whereas the original game skips it. This is a
false-positive detection: scripts that scan for valid living targets via
`Wld_DetectNpcEx` (e.g. guards re-assessing threats) will pick up bodies the
original ignores.

The HP>0 nuance and the transform-spell exclusion are additional, much rarer
edge cases; the practically observable gap is the missing unconscious filter,
which `Npc::isDown()` (= `isUnconscious() || isDead()`) covers exactly.

## Proposed patch

Grep-verified symbols: `Npc::isDown()` is declared public at
`game/world/objects/npc.h:286` and defined at
`game/world/objects/npc.cpp:4299` as `isUnconscious() || isDead()`.

OLD (`game/game/gamescript.cpp`, inside `wld_detectnpcex` lambda):
```cpp
       (guild==-1 || int32_t(n.guild())==guild) &&
       (&n!=npc) && !n.isDead() &&
       (player!=0 || !n.isPlayer())) {
```

NEW:
```cpp
       (guild==-1 || int32_t(n.guild())==guild) &&
       // NOTE: in original-game oCNpc::FindNpcEx @0x00740b80 the aliveOnly flag
       // (Wld_DetectNpcEx always passes 1) requires the target be alive AND not
       // unconscious (IsInState(-4)) AND not dead (IsInState(-5)); OpenGothic only
       // excluded the dead state, so knocked-out NPCs were wrongly detected.
       (&n!=npc) && !n.isDown() &&
       (player!=0 || !n.isPlayer())) {
```

`(&n!=npc)` (self-exclusion) is retained: it is a necessary OpenGothic
adaptation because `world().detectNpc` enumerates every NPC in `senses_range`,
whereas the original iterates the NPC's own perception list which does not
contain self.

## Externals checked

- **Wld_DetectNpcEx** — DIVERGENT (this finding): missing unconscious filter.
- **Wld_DetectNpc** (`FindNpc` @0x00740a80) — also divergent but **DEFERRED**:
  the original applies *no* liveness filter (detects dead NPCs), while
  OpenGothic adds `!n.isDead()`. Removing that filter would *add* detections
  (potential false positives) and the perception-list vs. radius-scan semantics
  already differ, so it is lower-confidence and intentionally not patched here.
- **Wld_DetectNpcEx player filter** — FAITHFUL: `(player!=0 || !n.isPlayer())`
  matches the original `(detectPlayer==0)→IsAPlayer==0` exclusion.
- **Wld_DetectItem** (`oCNpc::DetectItem` @0x0073fd40) — FAITHFUL, the prior
  `(main_flag|flags)` mask + `0x800000` no-detect skip is intact and correct.
- **Wld_IsFPAvailable** (`oCNpc::FindSpot` @0x007400e0) — radius is `700` (box
  half-extent) in the original vs OpenGothic's shared `distanceThreshold=800`
  sphere; **DEFERRED** (the threshold is shared across many way-matrix queries,
  so a localized 700 cap is not a surgical change).
- **Wld_GetGuildAttitude** (`oCGuilds::GetAttitude` @0x00700d40) — FAITHFUL,
  returns `ATT_NEUTRAL` on out-of-range (already fixed).
- **Wld_SetGuildAttitude / Wld_AssignRoomToGuild** — FAITHFUL.
- **Wld_RemoveNpc**, **Hlp_GetNpc** (`0x6eee10`), **Hlp_Random** (`0x6f7810`),
  **Game_InitGerman/English**, **PrintScreen** (`onPrintScreen`) — no
  not-yet-fixed divergence observed in this sweep.

(Note: handler bodies for `0x6f7810`/`0x6eee10` are not present in the warm
decompiler's analyzed-function set; the externals were assessed via their
DefineExternals registration and OpenGothic semantics, no divergence asserted.)
