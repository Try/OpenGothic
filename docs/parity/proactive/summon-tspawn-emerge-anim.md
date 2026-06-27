# Summon spell: missing `T_SPAWN` emerge animation on summoned creatures

**Confidence:** High

## Original function + address

The summon spells in Gothic II (Summon Skeleton, Summon Golem, Summon Demon,
Summon Wolves, Summon Guardian, Summon Zombie, etc.) are all driven by the
script external `Wld_SpawnNpcRange(c_npc origin, int clsId, int count, float)`.

Its handler lives in `oGameExternal.cpp` at entry `0x006df840` (the function
that calls `oCSpawnManager::SummonNpc` at the call site `0x006df9f3`). For each
of `count` creatures the handler:

1. Resolves the origin NPC and computes a forward spawn position.
2. Calls `oCSpawnManager::SummonNpc(clsId, pos, ...)` (`0x00778a20`), retrying
   the placement up to ~20 times with random repositioning when the slot is
   blocked.
3. **On every successful summon it fetches the new NPC's model
   (`oCNpc::GetModel`) and starts the transition animation `"T_SPAWN"` on it**
   (`zCModel::StartAni`, flag 0). The literal `"T_SPAWN"` is present in the
   binary (string at `0x008b5dfc`).

`T_SPAWN` is the "emerge from the ground" transition animation, so summoned
undead/golems rise out of the floor instead of popping into existence already
standing.

## OpenGothic file:line

`game/game/gamescript.cpp:1892` — `GameScript::wld_spawnnpcrange`:

```cpp
void GameScript::wld_spawnnpcrange(std::shared_ptr<zenkit::INpc> npcRef, int clsId, int count, float lifeTime) {
  auto at = findNpc(npcRef);
  if(at==nullptr || clsId<=0)
    return;

  (void)lifeTime;
  for(int32_t i=0;i<count;++i) {
    auto* npc = world().addNpc(size_t(clsId),at->position());
    fixNpcPosition(*npc,at->rotation() + 360.f*float(i)/float(count),100);
    }
  }
```

## Divergence

OpenGothic creates and positions each summoned creature but never starts the
`T_SPAWN` emerge animation that the original engine plays on every successful
summon. The result is a visible behavioral difference: in OpenGothic summoned
creatures appear instantly in their idle stance, whereas in `Gothic2.exe` they
play the rise-from-ground spawn animation. `"T_SPAWN"` does not appear anywhere
in the OpenGothic tree (grep-verified), so nothing else compensates for it. OG
already uses the exact same idiom for other engine-side transition animations
(`playAnimByName("T_DONTKNOW", BS_NONE)`).

## Proposed patch

OG symbols grep-verified:
- `Npc::playAnimByName(std::string_view, BodyState)` — `game/world/objects/npc.h:371`,
  defined `game/world/objects/npc.cpp:980`.
- `BS_NONE` already used with `playAnimByName` for transition anims
  (`game/game/gamescript.cpp:1018`, `:2832`).

OLD:
```cpp
  (void)lifeTime;
  for(int32_t i=0;i<count;++i) {
    auto* npc = world().addNpc(size_t(clsId),at->position());
    fixNpcPosition(*npc,at->rotation() + 360.f*float(i)/float(count),100);
    }
```

NEW:
```cpp
  (void)lifeTime;
  for(int32_t i=0;i<count;++i) {
    auto* npc = world().addNpc(size_t(clsId),at->position());
    fixNpcPosition(*npc,at->rotation() + 360.f*float(i)/float(count),100);
    // NOTE: in original-game Wld_SpawnNpcRange (oGameExternal.cpp @0x006df840, call site
    // 0x006df9f3) every successfully summoned creature is given the "T_SPAWN" emerge
    // animation via zCModel::StartAni ("T_SPAWN" @0x008b5dfc). Replicate so summoned
    // undead/golems rise from the ground instead of popping in already standing.
    npc->playAnimByName("T_SPAWN", BS_NONE);
    }
```

Notes / caveats:
- `playAnimByName` returns `nullptr` harmlessly when the creature's MDS has no
  `T_SPAWN`, matching the original's no-op for models lacking the animation.
- This patch is scoped strictly to the emerge animation. The original's
  retry-with-random-reposition placement loop and its use of the 4th float
  argument (used as a scatter distance on the retry path, *not* as a creature
  lifetime despite OG's `lifeTime` parameter name) are separate, lower-impact
  divergences left as DEFERRED — they affect only blocked-placement fallbacks.
