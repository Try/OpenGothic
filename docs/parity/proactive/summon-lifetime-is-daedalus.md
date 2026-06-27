# Summon subsystem: lifetime/count/despawn logic is pure Daedalus — NO FINDING (engine parity holds)

**Confidence:** N/A — NO FINDING (high confidence that no engine-side divergence exists in the
targeted summon lifetime / count-limit / despawn logic).

## Scope investigated
Summoned-creature (skeleton/golem/demon) lifetime timer, despawn-on-timeout, the
"one active summon" count limit, re-summon-replaces-old, follow-caster, ally attitude,
death-on-caster-death, and the `Npc_IsSummoned` / summon-owner link.

## Original functions examined (prose only, no code copied)
- `oCSpawnManager::SummonNpc` @ `0x00778a20` — the only engine function named for summoning. It
  RTTI-resolves an `oCNpc` from the instance id, sets it in the world, sets the active/insert-range
  bit (`0x2008` at `oCNpc+0x75c`) so the summon ticks immediately, calls `SpawnNpc`, and does a
  refcount `Release`. Its 4th argument is a `float` lifetime — but its single caller passes `0.0`.
- `Wld_SpawnNpcRange` external — the actual binding the Daedalus summon spells call. Two relevant
  bodies: the parser-stub `FUN_006df840` @ `0x006df840` (reads 3 params + self, calls `SummonNpc`
  with lifetime `0.0`) and `FUN_00483530` @ `0x00483530` (the alternate body emitting the
  `"C: Wld_SpawnNpcRange(): Monster Instance not found"` error @ `0x00896984`). In BOTH bodies the
  `lifeTime` parameter is never used to schedule any despawn; placement is done forward of the
  caster (~150 u) with up to 19 retries against `oCVob::SearchNpcPosition` + `FreeLineOfSight`, and
  the vob is removed from the world only if no valid position is found.
- `oCSpawnManager::CheckInsertNpc` @ `0x007780b0` and `CheckRemoveNpc` @ `0x007792e0` — the
  distance-based activate/deactivate system. They set/clear the `0x2000` bit at `oCNpc+0x75c` purely
  as a function of distance to the player; `CheckRemoveNpc` only acts on NPCs that hold a spawn-list
  slot (`oCNpc+0xb8 != 0`), which summons do not. So `0x2008` is a generic "active" marker, not a
  summon-specific lifetime hook.

## Findings
1. **Summon lifetime / despawn timer is Daedalus, not engine.** The engine never stores or counts
   down a summon TTL; `SummonNpc` is always invoked with lifetime `0.0`. OpenGothic's
   `wld_spawnnpcrange` (`game/game/gamescript.cpp:1912`) does `(void)lifeTime;` — this MATCHES the
   original, which also discards the parameter. Not a divergence.
2. **Count limit / re-summon-replaces-old / death-on-caster-death** are all Daedalus
   (`B_*Summon*` control via `self.aivar[...]`), with no engine support to diverge from.
3. **`Npc_IsSummoned` does not exist** as a vanilla Gothic 2 external (`wde strings IsSummoned` /
   `Summoned` return nothing; no `oCNpc::SetSummoned`/`Summoned` symbols in `functions.json`). Guild
   attitude for `GIL_SUMMONED_*` is driven by Daedalus `Gil_Attitudes` tables.
4. **Secondary, non-targeted observation (NOT the headline):** the only genuine engine-side
   difference is spawn PLACEMENT. The original spawns the creature ~150 u in front of the caster with
   line-of-sight retries and removes it if no valid spot is found; OpenGothic's `wld_spawnnpcrange`
   instead calls `world().addNpc(clsId, caster.position())` then `fixNpcPosition(npc,
   rotation+360*i/count, 100)`. For a single summon this is a minor cosmetic placement nuance, not
   summon lifetime/count/despawn logic, and is left DEFERRED.

## Verdict
DEFERRED / NO FINDING. The summon lifetime, count-limit, and despawn logic is implemented entirely
in Daedalus; the engine externals (`Wld_SpawnNpcRange` / `oCSpawnManager::SummonNpc`) ignore the
lifetime argument in both the original and OpenGothic, so engine parity holds. The only engine-level
delta is summon placement geometry, which is cosmetic and out of scope for this hunt.
