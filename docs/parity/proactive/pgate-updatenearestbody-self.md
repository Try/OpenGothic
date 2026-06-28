# Parity: `updateNearestBody` missing self-exclude (PERC_ASSESSBODY)

**Confidence:** High

## Original fn + address

`oCNpc::PerceptionCheck` (Gothic2.exe @0x0075dd30) is the per-frame perception
scan that classifies nearby vobs as enemy / body / player. Its candidate list is
built by `oCNpc::CreateVobList(float)` @0x0075da40, which after collecting vobs in
the bbox (`zCBspBase::CollectVobsInBBox3D`) walks the resulting list and explicitly
finds and removes the entry equal to `this`: it scans for `list[i] == this`, then
releases that element and swaps it out / decrements the count. So PerceptionCheck's
enemy *and* body classification never sees the scanning NPC itself. The body branch
(`hp < 1` OR `IsInState(-4)` unconscious, see `oCNpc::IsUnconscious` @0x00736750)
therefore can never select `this` as the nearest body.

## OG file:line

`/Users/admin/Downloads/opengothic/game/world/objects/npc.cpp:2388` —
`Npc::updateNearestBody()`, filter lambda at line 2400.

## Divergence

OG's `WorldObjects::detectNpcNear` (worldobjects.cpp:491) iterates `npcNear`
*including the calling NPC itself* — it performs no self-exclusion. The sibling
`Npc::updateNearestEnemy()` (npc.cpp:2375) correctly compensates with
`|| &n==this`, mirroring the original's self-strip in CreateVobList. But
`Npc::updateNearestBody()` (npc.cpp:2400) filters only `if(!n.isDown()) return;`
with **no `&n==this` check**. When the scanning NPC is itself down-but-not-dead
(unconscious / knocked out), it still runs the active perception loop
(worldobjects.cpp:247 skips only `isPlayer()`/`isDead()`, not unconscious), and
`detectNpcNear` hands it back to itself; `n.isDown()` is true, distance 0, so it
selects itself as the nearest body and fires `PERC_ASSESSBODY` (B_AssessBody) on
its own body — which the original engine structurally prevents.

## Proposed patch

OLD (npc.cpp:2400-2401):
```cpp
    if(!n.isDown())
      return;
```

NEW:
```cpp
    // NOTE: in original-game oCNpc::PerceptionCheck @0x0075dd30 builds its candidate
    // list via oCNpc::CreateVobList @0x0075da40, which strips the scanning NPC itself
    // from the collected vobs, so the body classification never selects `this`. An
    // unconscious (down, hp>0) NPC still runs the active perception loop, so without
    // this guard it would assess its own body. Matches updateNearestEnemy's `&n==this`.
    if(!n.isDown() || &n==this)
      return;
```

Surgical, build-verifiable, exactly mirrors the existing `updateNearestEnemy`
self-exclude one frame above.
