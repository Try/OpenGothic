# Wld_SpawnNpcRange: missing line-of-sight gate and remove-on-failure

> DEFER: the LoS + random-yaw retry + remove-on-failure loop is a multi-part behavioral change to monster spawning (could stop monsters spawning if mis-tuned); needs runtime validation.

Confidence: High (divergence exists), Medium (surgical fix completeness)

## Original function

`Wld_SpawnNpcRange` external (`ooCarsten.cpp`, FUN_00483530 @ 0x00483530).
Signature on the Daedalus side: `Wld_SpawnNpcRange(self, clsId, lifeTime, count)`.

For each of `count` monsters the original engine:
1. Creates the NPC instance (factory by symbol index; on failure it reports
   "C: Wld_SpawnNpcRange(): Monster Instance not found" and returns).
2. Picks an initial position by rotating the spawner's local frame by a
   *random* yaw (`PostRotateY(rand()*...)`) and offsetting along it.
3. Then loops up to **9 attempts**: each attempt re-randomizes a yaw, calls
   `oCVob::SearchNpcPosition` to snap to valid ground, and requires
   `oCNpc::FreeLineOfSight(player, npc)` to succeed; it `break`s on the first
   attempt that satisfies both.
4. If all 9 attempts fail, it calls `RemoveVobFromWorld` — the NPC is **not**
   spawned at all.

So in the original a ranged spawn only places monsters at positions that are
on valid ground *and* visible from the player, and silently drops monsters for
which no such spot is found within 9 tries.

## OpenGothic

`game/game/gamescript.cpp:1862` `wld_spawnnpcrange`:

```
for(int32_t i=0;i<count;++i) {
  auto* npc = world().addNpc(size_t(clsId),at->position());
  fixNpcPosition(*npc,at->rotation() + 360.f*float(i)/float(count),100);
  }
```

`fixNpcPosition` (`gamescript.cpp:764`) sweeps radius/angle for a non-colliding
ground spot but has no line-of-sight requirement and never removes the NPC on
failure (it just leaves it at the last tried position). It also distributes the
NPCs at evenly spaced angles instead of random yaws.

## Divergence (gameplay)

- OG always spawns all `count` monsters even when none can be placed within
  view of the player; original drops those it cannot place visibly within 9
  tries. This changes encounter difficulty and can spawn monsters behind walls
  / out of intended sight.
- Placement angles are deterministic/even in OG vs. random in original
  (cosmetic, lower severity).

## Proposed patch

```
// game/game/gamescript.cpp  (wld_spawnnpcrange)
// OLD
  (void)lifeTime;
  for(int32_t i=0;i<count;++i) {
    auto* npc = world().addNpc(size_t(clsId),at->position());
    fixNpcPosition(*npc,at->rotation() + 360.f*float(i)/float(count),100);
    }

// NEW
  (void)lifeTime;
  auto& pl = *world().player();
  for(int32_t i=0;i<count;++i) {
    auto* npc = world().addNpc(size_t(clsId),at->position());
    // NOTE: in original-game (ooCarsten.cpp Wld_SpawnNpcRange) each monster gets
    // up to 9 random-yaw placement attempts, each requiring valid ground AND
    // FreeLineOfSight(player). If none succeed the NPC is removed from the world.
    bool placed = false;
    for(int t=0; t<9 && !placed; ++t) {
      fixNpcPosition(*npc, float(std::rand())*(360.f/float(RAND_MAX)), 100);
      placed = pl.canSeeNpc(*npc,true);
      }
    if(!placed)
      world().removeNpc(*npc);
    }
```

(Exact LoS helper: `Npc::canSeeNpc` is the OG analogue of `FreeLineOfSight`;
adjust if a player handle is unavailable. The remove-on-failure branch is the
load-bearing parity change.)
