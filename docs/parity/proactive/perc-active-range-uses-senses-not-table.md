# Perc_SetRange table is applied to ACTIVE perceptions in OpenGothic, but the original bounds active perceptions only by `senses_range`

**Confidence:** Medium (code-level structural divergence is fully verified; observability depends on whether the loaded Daedalus scripts call `Perc_SetRange` for the active perception types 1..5, which could not be verified from material available in this environment).

## Original function + address (prose)

- `oCNpc::PerceptionCheck` (Gothic2.exe @0x0075dd30) drives the *active* assess perceptions
  (ASSESSPLAYER=1, ASSESSENEMY=2, ASSESSFIGHTER=3, ASSESSBODY=4, ASSESSITEM=5). It builds its
  candidate vob list with `CreateVobList(this, senses_range)` — i.e. the per-NPC `senses_range`
  field (oCNpc offset 0x284) is the *only* radius used to gather candidates for the active
  perceptions. The per-perception range table (`percRange`) is **never consulted** on the active
  path.
- `oCNpc::AssessPlayer_S` / `AssessEnemy_S` / `AssessFighter_S` / `AssessBody_S` / `AssessItem_S`
  (@0x0075a740 … 0x0075af40) — the per-type assess handlers selected afterwards — perform **no**
  range test at all; they only look the enabled perception's func up in the active-perception
  list and invoke it. So there is no secondary `percRange` filter hidden inside them either.
- `oCNpc::SetPerceptionRange` (the `Perc_SetRange` external's target, @0x0075e440) writes
  `percRange[perc] = value` into a single global float table (only writer is the script external).
- `oCNpc::CreatePassivePerception` (@0x0075b270) — by contrast — gathers its candidates with
  `CreateVobList(this, percRange[perc])`. So in the original the `percRange` table governs the
  **passive / sound** perceptions, while the **active** perceptions are governed by `senses_range`.

Net: in the original these are two independent radius sources. Active = `senses_range`. Passive =
`percRange[perc]` (with no `senses_range` fallback).

## OpenGothic file:line

- `game/world/objects/npc.cpp:4490` — `Npc::perceptionProcess(...)` (the active path, called for
  PERC_ASSESSPLAYER/ENEMY/BODY at npc.cpp:4456/4464/4474):
  ```cpp
  float r = float(world().script().percRanges().at(perc, hnpc->senses_range));
  r = r*r;
  if(quadDist>r) return false;
  ```
- `game/game/gamescript.cpp:80` — `PerDist::at()`:
  ```cpp
  int at(PercType perc, int r) const {
    if(perc>=PERC_Count) return r;
    auto rr = range[perc];
    if(rr>0) return rr;   // <- honor Perc_SetRange value
    return r;             // <- fall back to senses_range
    }
  ```

## Divergence

OpenGothic feeds the active perceptions through `percRanges().at(perc, senses_range)`, so a
`Perc_SetRange(PERC_ASSESSPLAYER/ENEMY/BODY, X)` made by the scripts will **override** the NPC's
`senses_range` for those active perceptions. The original ignores the table entirely for active
perceptions and always uses the per-NPC `senses_range`.

- If the loaded scripts never set ranges for the active types (1..5), `at()` returns the
  `senses_range` fallback and OpenGothic happens to match the original — no observable difference.
- If the loaded scripts *do* set a range for an active type that differs from `senses_range`
  (e.g. a global `PERC_DIST_ACTIVE_MAX` smaller/larger than a given NPC's `senses_range`),
  OpenGothic changes the player/enemy/body detection radius for that NPC while the original keeps
  using the NPC-specific `senses_range`.

(Note the related, but practically inert, second half of the same `at()` formula: for the *passive*
path `at()` substitutes `senses_range` when `percRange[perc]` is unset/<=0, whereas the original
`CreatePassivePerception` uses the raw table value with no fallback. This only matters for passive
perceptions a mod never set a range for, which never fire meaningfully in either engine, so it is
not independently observable.)

## Proposed patch — DEFERRED

Candidate surgical change (active perceptions should use `senses_range` directly, never the table):

```cpp
// game/world/objects/npc.cpp  Npc::perceptionProcess(...)  (active path)
// NOTE: in original-game oCNpc::PerceptionCheck @0x0075dd30 active assess perceptions
// (ASSESSPLAYER/ENEMY/FIGHTER/BODY/ITEM) gather candidates with CreateVobList(senses_range);
// the Perc_SetRange table (oCNpc::SetPerceptionRange @0x0075e440) is consulted only by the
// passive path (oCNpc::CreatePassivePerception @0x0075b270), never for active perceptions.
- float r = float(world().script().percRanges().at(perc, hnpc->senses_range));
+ float r = float(hnpc->senses_range);   // active perceptions ignore Perc_SetRange in vanilla
```

DEFERRED because:
1. The behavioral impact is conditional on the loaded Daedalus scripts actually calling
   `Perc_SetRange` for perception types 1..5 with a value different from `senses_range`; that
   trigger could not be verified here, so the change cannot be confirmed to alter observable
   behavior in vanilla G2 (and may be a no-op there).
2. `Npc::perceptionProcess(perc)` is shared with zero-distance callers (ASSESSTALK/ASSESSDAMAGE/
   ASSESSMAGIC) where the radius is irrelevant, and with ASSESSENEMY/ASSESSBODY whose candidate
   selection already happens in `updateNearestEnemy/Body`; a clean fix should restrict the source
   swap to exactly the active assess types and be validated against a real `Perc_SetRange` dump
   before changing detection radii (gameplay-affecting).

Grep-verified OG symbols used above: `Npc::perceptionProcess` / `percRanges()` / `PerDist::at`
(gamescript.cpp:80, gamescript.h:151) / `hnpc->senses_range` (npc.cpp:4490,4960-area) /
`perception[perc].func` (npc.cpp:4419).
