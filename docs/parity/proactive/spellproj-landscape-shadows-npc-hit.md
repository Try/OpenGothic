# Spell projectile detonates on landscape even when an NPC is closer along the same tick step

**Confidence:** Medium-High

## Original function + address (prose)

- `oCVisualFX::DoMovements` (Gothic2.exe `0x00497550`) advances a flying spell-projectile by
  calling the engine move (`zCVob::Move`) for the trajectory step. Collision during that move
  sweep is resolved by the ZenGin world/collision system, which reports the **closest**
  collider along the swept segment regardless of its type, and forwards it through
  `oCVisualFXAI::ReportCollisionToAI` (`0x0048bd70`) into the projectile's collision handler
  `oCVisualFX::ProcessCollision` (`0x004958d0`).
- `oCVisualFX::ProcessCollision` (`0x004958d0`, see also sibling note
  `spellhit-magrange-cutoff.md`) applies damage purely on **physical collision**: the magic
  ball flies until it physically hits *something* — a vob (NPC) or world geometry — and the
  collider that is hit is whichever the move sweep finds first along the path. There is no rule
  that world geometry is privileged over an NPC standing in front of it; the nearer collider
  wins.

The net original behaviour: if, within a single movement step, the spell's path passes through
an NPC *and* then strikes a wall behind that NPC, the NPC (being closer) is the collider — the
spell damages the NPC, not the wall.

## OpenGothic file:line

`game/physics/dynamicworld.cpp:918-956` (`DynamicWorld::moveBullet`, the
`if(callback.matId != NONE)` / `if(isSpell)` branch).

## Divergence

`moveBullet` performs the per-tick landscape raycast (`world->rayCast`, line 916) and the
NPC sweep (`npcList->rayTest`, line 958) in **mutually-exclusive** branches:

```
if(callback.matId != NONE) {          // landscape was hit this step
  if(isSpell){
    if(b.cb!=nullptr)
      b.cb->onCollide(callback.matId); // -> Bullet::onCollide(matId): detonate on wall
    stopBullet = true;
  } else { ...arrow ricochet/stop... }
} else {                               // landscape NOT hit this step
  if(auto ptr = npcList->rayTest(pos,to,b.targetRange(),nullptr)) { ... } // only here
  ...
}
```

For a spell, the NPC collision test (`npcList->rayTest`) is reached **only when no landscape
was hit during the step**. The moment the step ray strikes any wall/terrain, the spell
detonates on that wall and the NPC test is skipped entirely — even when an NPC lies closer
along the same segment (`callback.m_closestHitFraction` is available and could be compared, but
is not). The two hit distances are never compared.

This is spell-distinct in impact: spell projectiles use a wide auto-aim collision radius
(`b.targetRange()` = `vfx->emTrjTargetRange`, set in `World::shootSpell`,
`game/world/world.cpp:648`) and are typically cast directly at NPCs, who frequently stand in
front of walls, pillars, or sloped terrain. At low frame-rate (large `dt`, hence a long step)
the spell can sail through the target's hit-sphere and detonate on the geometry behind it,
dealing zero damage. The original would have registered the closer NPC.

(Arrows share the same branch structure, but the practical exposure differs: arrows have
`targetRange()==0` — a tight hit-sphere — and an arc, so the "wall behind the target in the
same step" geometry is far rarer. The proposed fix below is gated to the spell branch only, to
stay surgical and avoid touching the arrow ricochet path.)

## Proposed patch

Before detonating a spell on landscape, sweep for an NPC within the segment **truncated to the
landscape hit point**; if one is closer, collide with the NPC instead. All symbols used below
are already present in this function/file: `callback.m_closestHitFraction` (used at
dynamicworld.cpp:938,948), `b.targetRange()` (line 958), `b.cb` / `BulletCallback::onCollide(Npc&)`
(lines 959-960), `npcList->rayTest(pos,e,extR,except)` (line 958), `NpcBody::toNpc()` (line 79).

OLD (`game/physics/dynamicworld.cpp`, the `isSpell` true-branch at ~920-923):
```cpp
    if(isSpell){
      if(b.cb!=nullptr)
        b.cb->onCollide(callback.matId);
      stopBullet = true;
      } else {
```

NEW:
```cpp
    if(isSpell){
      // NOTE: in original-game oCVisualFX::DoMovements @0x00497550 the projectile move sweep
      // reports the CLOSEST collider of any type; an NPC in front of a wall is hit before the
      // wall. OpenGothic resolves landscape and NPC hits in mutually-exclusive branches, so a
      // landscape hit would otherwise shadow a closer NPC within the same tick step.
      const auto hitPos = pos + (to-pos)*callback.m_closestHitFraction;
      if(auto ptr = npcList->rayTest(pos,hitPos,b.targetRange(),nullptr)) {
        if(b.cb!=nullptr)
          stopBullet |= b.cb->onCollide(*ptr->toNpc()); else
          stopBullet  = true;
        }
      if(!stopBullet) {
        if(b.cb!=nullptr)
          b.cb->onCollide(callback.matId);
        stopBullet = true;
        }
      } else {
```

Rationale for correctness: `npcList->rayTest(s,e,extR,except)` already returns the **nearest**
NPC within `[s,e]` (it tracks `tHit` / `proj`, lines 282-307). Truncating `e` to the landscape
hit point `hitPos` guarantees we only pre-empt the wall detonation when an NPC is genuinely
closer than the wall along this step; if no closer NPC exists we fall through to the original
landscape detonation unchanged. `b.cb->onCollide(Npc&)` returns whether the spell stopped
(`Bullet::onCollide(Npc&)` returns true on a real hit), matching the existing non-spell NPC
path at lines 959-960.
