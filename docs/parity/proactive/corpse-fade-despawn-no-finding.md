# Corpse removal / body-fade / despawn — NO FINDING (subsystem already covered or non-surgical)

**Confidence:** N/A — NO FINDING (high confidence that no *new* high-confidence surgical
divergence remains in the targeted corpse-removal / body-fade / despawn logic).

## Scope investigated
The dead-body-stays-forever vs fade timer, the engine `ZS_FADEAWAY` body-fade-out, the
looted-corpse removal, the monster-corpse despawn-at-range, the spawn-manager respawn timer,
body cleanup on level change, perception-disable on corpse, and dropped-loot persistence.

## Original functions examined (prose only, no code copied)
- `oCNpc::StartFadeAway` @ `0x00736d40` / `oCNpc::FadeAway` @ `0x00736e40` — the engine body
  fade-out. StartFadeAway tears down the NPC's spawn-list slot block (`+0x998`/`+0x9a0`),
  enters built-in AI state `-5` (`ZS_FADEAWAY`, string @ `0x008b8388`), seeds a fade timer
  (`+0x9c8` = total duration `DAT_0083d704`) and stores `routine-delay*1000` (`+0x27c`) into
  `+0x9cc`. FadeAway ramps the body alpha (`+0xcc` = remaining/total) each tick and, on expiry,
  calls the world `RemoveVob` plus `oCSpawnManager::DeleteNpc`. No standard caller of
  StartFadeAway is reachable via xrefs (the warm decompiler reports none), and the path is tied
  to the spawn-list teardown — i.e. the spawn/respawn manager, not the normal NPC death path.
- `oCSpawnManager::CheckRemoveNpc` @ `0x007792e0` — distance-based corpse/NPC removal. Acts only
  on NPCs that hold a spawn-list slot (`+0xb8 != 0`), have no pending events, are dead or
  condition-invalid, and are beyond `SPAWN_REMOVERANGE` from the player (and from their
  routine/AI position). It deletes or re-spawns; it does **not** go through `StartFadeAway`.
- `oCGame::DeleteNpcs` @ `0x006c8e80` — range-gated bulk RemoveVob of non-player NPCs; a
  world/transition cleanup helper, not a corpse-specific timer.
- `oCNpc::DoDie` @ `0x00736760` / `oCNpc::DropUnconscious` @ `0x00735eb0` — the actual death and
  knock-out handlers (collision flags, perception table, loot) — already analysed in prior docs.

## Why NO new finding
Every concrete divergence in this subsystem is already documented:
- **Body fade-out / despawn-at-range / respawn timer** belong to the `oCSpawnManager` subsystem
  that OpenGothic intentionally does not implement. Already recorded as DEFERRED in
  `spawn-range-los-remove.md`, `spawn-insertnpcandrespawn-missing.md`,
  `spawn-wld-insertnpcandrespawn-unbound.md`. The `ZS_FADEAWAY` fade has no reachable engine
  trigger outside that spawn-list teardown, and there is no OpenGothic hook to patch surgically.
- **Summon lifetime / despawn** is Daedalus-driven in both engines — `summon-lifetime-is-daedalus.md`
  (NO FINDING) and `summon-tspawn-emerge-anim.md`.
- **Looted-corpse contents / armour hidden** — `death-corpse-armor-loot.md`.
- **Corpse collision (walk-through / projectile-pass-through)** — `death-corpse-physics-disabled.md`
  (DEFERRED). Note: `game/world/objects/npc.cpp:622` now calls `physic.setEnable(false)`
  unconditionally (both death and unconscious), which keeps that already-documented tension with
  `oCNpc::DoDie`'s tail `SetPhysicsEnabled(this,1)`; it is the same root cause, not a new one.
- **Perception wipe on corpse (death vs unconscious asymmetry)** — `death-unconscious-perc-wipe.md`
  (DEFERRED), and `body-downed-player-still-assessed-as-player.md` / `perc2-assessbody-unconscious.md`.

## Verdict
NO FINDING. For normal NPCs, corpses persist in both engines (no fade); the fade/despawn/respawn
behaviours that differ are part of the deliberately-unimplemented `oCSpawnManager` subsystem and
have no surgical, build-verifiable single-symbol fix. No new high-confidence divergence to report.
