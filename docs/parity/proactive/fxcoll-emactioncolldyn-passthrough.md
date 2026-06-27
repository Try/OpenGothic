# Spell projectile always stops on NPC hit — ignores `emActionCollDyn` COLLIDE bit

**Confidence:** High (on the divergence); Medium-High (on the patch — see CREATEONCE caveat)

## Original function + address (prose only)

- `oCVisualFX::ProcessCollision` @ `0x004958d0`. Its return value is the single
  expression "`emActionCollDyn` flags AND the `COLLIDE` bit", i.e. it returns non-zero only
  when the dynamic-collision action string for that FX contains `COLLIDE`. The dynamic action
  flags are parsed in `oCVisualFX::ParseStrings` @ `0x0048be60`: the dynamic-action string is
  searched for `BOUNCE` (bit `2`), `CREATEONCE` (bit `8`), `CREATE` (bit `4`) and `COLLIDE`
  (bit `1`); the field is the dynamic-collision flag word read in `ProcessCollision` as
  `this+0x39c`.
- `oCVisualFX::ProcessQueuedCollisions` @ `0x00495830` ORs the return value of every
  `ProcessCollision` call this frame and, only if that OR is non-zero, invokes the FX's
  `Collide` virtual (`oCVisualFX::Collide` @ `0x00493a00`), which sets the FX state to its
  collide-key and calls `EndEffect` — i.e. it terminates / consumes the flying projectile FX.
- Consequence in the original: a spell projectile whose `emActionCollDyn` is `CREATE`,
  `CREATEONCE`, `BOUNCE`, `NORESP` or empty (no `COLLIDE` bit) does **not** end on hitting an
  NPC; it keeps flying and can damage further targets (pierce / chain). Only `COLLIDE` ends it.
  (`CREATEONCE` additionally adds the hit vob to the per-FX ignore list `this+0x400`, consulted
  by `oCVisualFX::CanThisCollideWith` @ `0x00496ac0`, so each target is hit at most once.)

## OpenGothic file:line

`game/world/bullet.cpp:141` — `bool Bullet::onCollide(Npc& npc)`

The function unconditionally `return true;` (line 159). Its caller
`DynamicWorld::moveBullet` (`game/physics/dynamicworld.cpp:960`,
`stopBullet |= b.cb->onCollide(*ptr->toNpc());`) uses that boolean as the
"stop the bullet" flag, so every spell that touches an NPC is consumed on the first hit. The
return value is also consumed by `Bullet::onEffectCollide` (`bullet.cpp:110-115`) to trigger
`onStop()`.

## Divergence

OpenGothic always treats an NPC hit as terminal (`return true`), regardless of the spell FX's
`emActionCollDyn` action. The original terminates the projectile only when
`emActionCollDyn & COLLIDE`. Spells configured as `CREATE` / `CREATEONCE` / `BOUNCE`
(pass-through / chain / pierce behaviour) are therefore stopped on the first NPC in OpenGothic
instead of flying on to subsequent targets.

The required field already exists and parses identically for the relevant bit:
`VisualFx::emActionCollDyn` (`game/graphics/visualfx.h:133`, type `Collision`) with
`Collision::Collide = 1` (`visualfx.h:20`); `VisualFx::strToColision`
(`visualfx.cpp:234`) sets the `Collide` bit from the word `"COLLIDE"`, matching the original.
`Effect::handle()` (`effect.h:30`) exposes the spell's root `VisualFx*`.

## Proposed patch

`game/world/bullet.cpp`, in `Bullet::onCollide(Npc& npc)`. Read the action bit before `vfx`
is moved out, and skip the "consume the projectile" path (setKey/runEffect/return-true) when
the FX is a pass-through spell:

```cpp
// OLD
bool Bullet::onCollide(Npc& npc) {
  if(&npc==origin() || isFinished())
    return false;

  if(ow!=nullptr) {
    // no damage between ally npc's, only emit pfx effect
    const bool friendlyFire = wrld->script().isFriendlyFire(*ow, npc);
    if(!friendlyFire) {
      if(isSpell())
        npc.takeDamage(*ow,this,vfx.handle(),spellId()); else
        npc.takeDamage(*ow,this);
      }
    }
  vfx.setKey(*wrld,SpellFxKey::Collide);
  vfx.setLooped(false);
  vfx.setPhysicsDisable();
  wrld->runEffect(std::move(vfx));

  return true;
  }
```

```cpp
// NEW
bool Bullet::onCollide(Npc& npc) {
  if(&npc==origin() || isFinished())
    return false;

  if(ow!=nullptr) {
    // no damage between ally npc's, only emit pfx effect
    const bool friendlyFire = wrld->script().isFriendlyFire(*ow, npc);
    if(!friendlyFire) {
      if(isSpell())
        npc.takeDamage(*ow,this,vfx.handle(),spellId()); else
        npc.takeDamage(*ow,this);
      }
    }

  // NOTE: in original-game oCVisualFX::ProcessCollision @0x004958d0 the return value is
  // (emActionCollDyn & COLLIDE); ProcessQueuedCollisions @0x00495830 only ends the flying FX
  // (oCVisualFX::Collide @0x00493a00 -> EndEffect) when that bit is set. Spell FX whose
  // emActionCollDyn is CREATE/CREATEONCE/BOUNCE/NORESP (no COLLIDE bit) pass through the npc
  // and keep flying to reach further targets. Non-spell bullets (arrows) keep stopping.
  const VisualFx* root = vfx.handle();
  const bool stop = !isSpell() || root==nullptr ||
                    (root->emActionCollDyn & VisualFx::Collide)!=0;
  if(!stop)
    return false;

  vfx.setKey(*wrld,SpellFxKey::Collide);
  vfx.setLooped(false);
  vfx.setPhysicsDisable();
  wrld->runEffect(std::move(vfx));

  return true;
  }
```

### Notes / residual risk

- The fix is surgical (one function, all symbols grep-verified) and build-safe.
- The original prevents a `CREATEONCE` projectile from re-hitting the **same** target via the
  per-FX ignore list (`this+0x400`, `CanThisCollideWith` @ `0x00496ac0`). OpenGothic has no such
  per-bullet ignore list, so in principle a slow pass-through projectile could damage the same
  NPC on consecutive frames. In practice `DynamicWorld::moveBullet` advances the bullet to `to`
  every frame and the per-frame ray returns only the closest single NPC, so the projectile
  marches past the hit NPC; repeat-hits are an edge case, and plain `CREATE` re-hits are
  original behaviour anyway. Adding a per-bullet hit-set to exactly model `CREATEONCE` is a
  reasonable follow-up but is out of scope for this surgical change.
