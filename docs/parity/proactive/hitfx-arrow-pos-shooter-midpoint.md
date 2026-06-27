# Ranged (arrow/bolt) weapon-hit FX spawns at the shooter↔victim midpoint instead of the impact point

**Confidence:** Medium

## Original function + address (prose only)

The original positions the on-hit collision spark/sound at (or next to) the
actual point of contact, not at a point interpolated between the two
combatants. For melee the damage descriptor's hit position is taken from the
attacker's weapon hit-node, computed in `oCAniCtrl_Human::CreateHit`
(Gothic2.exe @0x006b0830): the per-hit world position is filled from the
victim's node fields (or, when the "use own node" flag is clear, from the
attacker's own hit node at `this+0x188`), and that single point is what
`oCNpc::OnDamage_Effects_Start` (Gothic2.exe @0x0066ee40) later hands to
`zCVob::SetPositionWorld` for the spawned blood/hit vob (it reads the hit
position straight out of the damage descriptor at `+0x54`). The engine's
collision-driven hit/spark sound (`zCSoundManager::StartHitSound`
@0x005ecae0, reached via `zCAIBaseSound::StartDefaultCollisionSound`
@0x0050c1d0) likewise derives its emit position from the collision report's
contact point / the moving vob's bbox, never from the midpoint between the
striker and the struck object. For a flying arrow the striking vob is the
projectile itself, so the contact point sits on the victim, far from the
archer. In every original code path the effect therefore appears at the
impact, which for a ranged hit means "at the victim", never half-way back
toward the shooter.

## OpenGothic file:line

`game/world/world.cpp:730` — `World::addWeaponHitEffect(Npc& src, const Bullet* srcArrow, Npc& reciver)`

Specifically lines 731-736 (position) and the arrow branch 746-749.

## Divergence

`addWeaponHitEffect` computes a single spawn position as the **midpoint of the
two NPC centers**:

```cpp
auto p0 = src.centerPosition();        // = *ow, the shooter (bullet origin)
auto p1 = reciver.centerPosition();    // the victim
pos.translate((p0+p1)*0.5f);
```

and reuses that `pos` for the arrow branch as well:

```cpp
if(srcArrow!=nullptr && !srcArrow->isSpell()) {
  auto m = ItemMaterial(srcArrow->itemMaterial());
  return addHitEffect(materialTag(m),armor,"IAM",pos);
  }
```

For a melee strike the two combatants are adjacent, so the midpoint is a fine
approximation of the contact point. But for a ranged hit the call comes from
`Npc::takeDamage(Npc& other, const Bullet* b)` → `addWeaponHitEffect(other,b,*this)`
(`game/world/objects/npc.cpp:2163`), where `other` is the bullet's origin
(`Bullet::ow`, set in `Bullet::onCollide(Npc&)`, `game/world/bullet.cpp:150-151`)
— i.e. the **archer**, who can be many metres away. The `CS_IAM_*` hit sound and
the `CPFX_IAM_*` spark/blood particle are then spawned floating in mid-air,
roughly half-way between the archer and the target, instead of on the victim
where the arrow landed. (The land-hit sibling path is already correct: when an
arrow strikes the level/an object, `Bullet::onCollide(MaterialGroup)` positions
the effect at the arrow's own matrix, `game/world/bullet.cpp:131`.)

## Proposed patch

Position the arrow-hit FX at the impact (the victim) rather than the
shooter↔victim midpoint. The minimal, grep-verified fix uses the already-bound
`p1` (victim centre) for the arrow branch; the symbols `Tempest::Matrix4x4`,
`.identity()`, `.translate()` and `p1` are all in use immediately above.

OLD (`game/world/world.cpp:746-749`):
```cpp
  if(srcArrow!=nullptr && !srcArrow->isSpell()) {
    auto m = ItemMaterial(srcArrow->itemMaterial());
    return addHitEffect(materialTag(m),armor,"IAM",pos);
    }
```

NEW:
```cpp
  if(srcArrow!=nullptr && !srcArrow->isSpell()) {
    // NOTE: in original-game oCAniCtrl_Human::CreateHit @0x006b0830 / OnDamage_Effects_Start
    // @0x0066ee40 the hit FX is spawned at the impact point (the struck vob), never at the
    // striker<->victim midpoint; for a flying arrow the striker is the projectile, so the spark
    // belongs on the victim, not half-way back to the archer.
    Tempest::Matrix4x4 apos;
    apos.identity();
    apos.translate(p1);
    auto m = ItemMaterial(srcArrow->itemMaterial());
    return addHitEffect(materialTag(m),armor,"IAM",apos);
    }
```

Ideal (closer to vanilla, but needs a small new accessor — therefore noted, not
applied here): expose the projectile's world position via a `Bullet::position()`
getter returning `obj->position()` (the private `DynamicWorld::BulletBody* obj`
already exposes `position()`, used at `game/world/bullet.cpp:135`) and translate
`apos` by that instead of `p1`. This lands the spark exactly on the arrow's
contact point. The victim-centre approximation above is chosen as the surgical,
no-new-API change.

Scope note: the melee branches (lines 751-758) are intentionally left on the
midpoint — combatants are adjacent there and the existing approximation matches
the original closely; only the ranged branch is clearly wrong.
