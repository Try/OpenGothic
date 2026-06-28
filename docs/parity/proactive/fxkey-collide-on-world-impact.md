# FX-key parity: COLLIDE key not applied to spell FX on world-geometry impact

**Confidence:** Medium-High

## Original fn + address
In Gothic2.exe both spell-collision dispatch paths funnel into the single virtual
`oCVisualFX::Collide` @ `0x00493a00`:

- NPC / dynamic-vob hit: `oCVisualFX::ProcessCollision` @ `0x004958d0` ->
  `oCVisualFX::ProcessQueuedCollisions` @ `0x00495830` -> `Collide`.
- Level-polygon / static-geometry hit: `oCVisualFX::ReportCollision` @ `0x00494e80`
  -> `Collide`.

`oCVisualFX::Collide` does three things, unconditionally, for *both* collision
sources: it propagates `Collide` (vtable +0x130) to its child FX, then it looks up
`<instance>_KEY_COLLIDE`, and if found assigns it (`this+0x4e0`) and calls
`UpdateFXByEmitterKey` — i.e. it switches the dying flying FX onto its COLLIDE
emitter key (collide color/mesh/alpha) — and finally sets state 6 and `EndEffect`.
So in the original, the projectile FX always morphs into its COLLIDE appearance the
instant it stops, regardless of whether it hit an NPC or a wall.

(The emFXCollStat / emFXCollDyn impact sub-FX are spawned by ProcessCollision /
ReportCollision *before* `Collide`, which is why the COLLIDE key on the main FX and
the spawned impact sub-FX are independent effects in OG.)

## OG file:line
`/Users/admin/Downloads/opengothic/game/world/bullet.cpp:142-153`
(`Bullet::onCollide(zenkit::MaterialGroup)` — the level/world-mesh impact path).

Compare with the NPC-hit overload `Bullet::onCollide(Npc&)` at
`bullet.cpp:180`, which *does* call `vfx.setKey(*wrld,SpellFxKey::Collide)`.

## Divergence
The two OpenGothic collision overloads are asymmetric:

- NPC hit (`onCollide(Npc&)`): spawns the emFXCollDyn impact sub-FX (via
  `takeDamage` -> `Effect::onCollide`, npc.cpp:2181) **and** applies the COLLIDE key
  to the main flying FX (`vfx.setKey(...,SpellFxKey::Collide)`, bullet.cpp:180).
  Matches OG.
- World hit (`onCollide(MaterialGroup)`): spawns the emFXCollStat impact sub-FX
  (`Effect::onCollide`, bullet.cpp:148) but **never** applies the COLLIDE key to the
  main flying FX before `runEffect(std::move(vfx))`. The projectile FX plays out its
  death still on its CAST/flying emitter key instead of switching to `_KEY_COLLIDE`.

So a spell that stops on level geometry (emActionCollStat carries the COLLIDE bit)
misses the COLLIDE key-stage on its primary FX — a discrete key-lifecycle omission,
not a visual-feel issue. The gating already matches OG: this branch is only reached
when the FX actually stops (COLLIDE bit set), exactly when OG would call
`oCVisualFX::Collide`.

This is distinct from the four excluded items (it is the COLLIDE *key application*
on the world-impact path, not the collide damage-level, trail-open-key,
emSelfRotVel, or light range).

## Proposed patch
In `Bullet::onCollide(zenkit::MaterialGroup matId)`, apply the COLLIDE key to the
main FX before handing it off, mirroring the NPC overload.

OLD (`game/world/bullet.cpp`, ~line 148):
```cpp
  Effect::onCollide(*wrld,vfx.handle(),obj->position(),nullptr,ow,spellId());
  vfx.setLooped(false);
  vfx.setPhysicsDisable();
  wrld->runEffect(std::move(vfx));
  return true;
```

NEW:
```cpp
  Effect::onCollide(*wrld,vfx.handle(),obj->position(),nullptr,ow,spellId());
  // NOTE: in original-game oCVisualFX::Collide @0x00493a00 (reached for BOTH collision
  // sources: ProcessCollision @0x004958d0 for dyn-vob hits and ReportCollision
  // @0x00494e80 for level-polygon hits) the dying flying FX is switched onto its
  // <instance>_KEY_COLLIDE emitter key via UpdateFXByEmitterKey before EndEffect.
  // The NPC-hit overload below already does this (vfx.setKey(...,Collide)); the
  // level-mesh path omitted it, so a spell stopping on world geometry kept its CAST
  // key appearance instead of morphing to its collide visual.
  vfx.setKey(*wrld,SpellFxKey::Collide);
  vfx.setLooped(false);
  vfx.setPhysicsDisable();
  wrld->runEffect(std::move(vfx));
  return true;
```

Build-verifiable: `setKey(World&, SpellFxKey, int32_t=0)` exists
(`game/graphics/effect.h:41`); `SpellFxKey::Collide` exists
(`game/game/constants.h:278`). Same call already compiled at bullet.cpp:180.
