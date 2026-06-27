# Wld_SpawnNpcRange — missing line-of-sight gate and cull-on-failure

**Confidence:** Medium-High (original behavior verified directly; fix is non-surgical → DEFERRED)

## Original function + address
The script external `Wld_SpawnNpcRange` is registered in `oCGame::DefineExternals_Ulfi`
(Gothic2.exe) with the parser thunk at `0x006df840`; the actual spawn logic lives in
the implementation at `0x00483530` (the function that owns the
`"C: Wld_SpawnNpcRange(): Monster Instance not found"` diagnostic string at `0x00896984`).

Behavior of the original, in prose (per the decompiled implementation at `0x00483530`):

- It loops over `count` instances. For each it creates an `oCNpc` vob for the requested
  monster instance; if the instance cannot be created it reports the "Monster Instance not
  found" error and aborts.
- Each spawned NPC is given an **initial** placement near the anchor vob by rotating the
  anchor's matrix by a **random** Y angle and offsetting along that heading (not a fixed
  ring), then it is added to the world.
- It then runs a **placement search of up to 9 attempts**: each attempt random-rotates the
  anchor heading, calls `oCVob::SearchNpcPosition` to snap to valid walkable ground, and
  then requires `oCNpc::FreeLineOfSight(player, npc)` to succeed. The first attempt that
  yields a valid position **with a free line of sight to the player** is accepted.
- If none of the 9 attempts produces a valid line-of-sight position, the NPC is
  **removed from the world** (`RemoveVobFromWorld`). I.e. NPCs that cannot be placed where
  the player can see them are culled, so the actual spawned count can be **less than**
  `count`.

## OpenGothic file:line
`game/game/gamescript.cpp:1930` — `GameScript::wld_spawnnpcrange(...)`, which calls
`fixNpcPosition(*npc, at->rotation() + 360.f*float(i)/float(count), 100)` per NPC
(`game/game/gamescript.cpp:1938`). `fixNpcPosition` is defined at
`game/game/gamescript.cpp:783`.

## Divergence
1. **Angle distribution:** OG places NPCs on a *deterministic* ring
   (`at->rotation() + 360*i/count`); the original uses *random* per-attempt headings.
2. **Line-of-sight gate (the substantive one):** OG's `fixNpcPosition` only spiral-searches
   for the nearest **collision-free** spot (raycast + `hasCollision`). It never checks line
   of sight to the player. The original additionally requires `FreeLineOfSight(player, npc)`,
   so the original deliberately spawns creatures in spots the player can see, whereas OG can
   place them on the far side of a wall as long as that spot is collision-free.
3. **Cull-on-failure:** OG never removes an NPC that cannot be validly placed — when
   `fixNpcPosition` fails it leaves the NPC at the last tried position (note the restore line
   `// npc.setPosition(pos0);` at `gamescript.cpp:809` is even commented out). The original
   removes such an NPC, so OG always materializes exactly `count` NPCs while the original may
   spawn fewer.

Net gameplay effect: summon-type spells / scripted ambushes using `Wld_SpawnNpcRange` can,
in OG, drop monsters behind geometry or out of sight and never cull unplaceable ones,
diverging from the original's "visible, line-of-sight-validated, possibly-fewer" spawn set.

## Proposed patch
**DEFERRED.** A faithful fix is not surgical: it requires reproducing
`oCVob::SearchNpcPosition` (ground/obstacle-aware candidate placement) plus a
`FreeLineOfSight(player, npc)` predicate and the removal of NPCs that fail all attempts,
none of which exist as a drop-in in OG's `fixNpcPosition`/`World` surface. Implementing the
LOS gate and cull without those primitives would be guesswork and risks regressing the
existing (intentional) collision-only placement. Recommend deferring until an
LOS/`SearchNpcPosition` equivalent is available.

<!-- NOTE: in original-game Wld_SpawnNpcRange impl @0x00483530 (thunk @0x006df840): each
     spawned oCNpc is placed via up to 9 random SearchNpcPosition attempts gated on
     FreeLineOfSight(player,npc), and removed via RemoveVobFromWorld if no attempt yields a
     line-of-sight position. -->
