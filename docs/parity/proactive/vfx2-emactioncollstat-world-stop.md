# VFX: spell FX ignores emActionCollStat COLLIDE bit when hitting level geometry

**Confidence:** High

## Original function + address

`oCVisualFX::ReportCollision` @ `0x00494e80`.

When the engine reports a collision for a flying FX it branches on the collision
object's class. If the other object is the static world mesh
(`zCCollObjectLevelPolys::s_oCollObjClass`), it enters the level-polygon branch and
consults the FX's `emActionCollStat` flag word (stored at `this+0x3a0`). Inside that
branch the FX is only *stopped/ended* — `zCVob::ResetToOldMovementState` followed by
`local_ac = 1`, which then invokes the FX's terminate slot (vtable `0x130` →
`oCVisualFX::Collide` @ `0x00493a00` → `EndEffect` @ `0x00494c80`) — when the COLLIDE
bit (`this+0x3a0 & 1`) is set. An FX whose `emActionCollStat` is
`CREATE`/`BOUNCE`/`NORESP`/`CREATEQUAD` without the COLLIDE bit is **not** terminated on
a level-mesh hit; it keeps flying. This is the static-collision twin of
`oCVisualFX::ProcessCollision` @ `0x004958d0`, whose dynamic-collision return is
`emActionCollDyn & 1` (the already-fixed `emActionCollDyn` COLLIDE pass-through in
`Bullet::onCollide(Npc&)`).

The Collision bit values match OpenGothic's `VisualFx::Collision` enum
(`Collide = 1`), and `emActionCollStat` is the grep-verified field
`game/graphics/visualfx.h:134`.

## OpenGothic file:line

- `game/physics/dynamicworld.cpp:919-923` — spell branch of the bullet raycast.
- `game/world/bullet.cpp:126-139` — `Bullet::onCollide(zenkit::MaterialGroup)`.
- `game/physics/dynamicworld.h:174` — `BulletCallback::onCollide(MaterialGroup)`.
- `game/world/bullet.h:64` — `Bullet::onCollide(MaterialGroup)` override.

## Divergence

In `dynamicworld.cpp` a spell bullet that raycasts into world geometry
(`callback.matId != NONE`) unconditionally sets `stopBullet = true` and
`Bullet::onCollide(matId)` unconditionally terminates the FX (spawns the
`emFXCollStat` child via `Effect::onCollide`, disables physics, moves the FX into
`runEffect`). OpenGothic therefore always stops a spell on the first level-mesh hit,
regardless of `emActionCollStat`. The original only stops when
`emActionCollStat & COLLIDE`; otherwise the spell passes through (and BOUNCE-class FX
ricochet). This is exactly symmetric to the dynamic-vob fix already applied in
`Bullet::onCollide(Npc&)`, which gates the stop on `emActionCollDyn & VisualFx::Collide`.

Non-spell bullets (arrows) are unaffected: their stop is driven by the
`else` (non-spell) branch and by `!isSpell()` evaluating `true` in the gate below.

## Proposed patch

Mirror the existing `Bullet::onCollide(Npc&)` pattern: make the level-mesh callback
return whether the bullet stopped, gated on the static-collision COLLIDE bit.

### 1. `game/physics/dynamicworld.h:174`

OLD:
```cpp
      virtual void onCollide(zenkit::MaterialGroup matId){(void)matId;}
```
NEW:
```cpp
      virtual bool onCollide(zenkit::MaterialGroup matId){(void)matId; return true;}
```

### 2. `game/physics/dynamicworld.cpp:919-923`

OLD:
```cpp
  if(callback.matId != zenkit::MaterialGroup::NONE) {
    if(isSpell){
      if(b.cb!=nullptr)
        b.cb->onCollide(callback.matId);
      stopBullet = true;
      } else {
```
NEW:
```cpp
  if(callback.matId != zenkit::MaterialGroup::NONE) {
    if(isSpell){
      if(b.cb!=nullptr)
        stopBullet = b.cb->onCollide(callback.matId); else
        stopBullet = true;
      } else {
```
(The two arrow call sites at lines ~945 and ~953 ignore the new return value and keep
their existing `stopBullet` handling.)

### 3. `game/world/bullet.h:64`

OLD:
```cpp
    void     onCollide(zenkit::MaterialGroup matId) override;
```
NEW:
```cpp
    bool     onCollide(zenkit::MaterialGroup matId) override;
```

### 4. `game/world/bullet.cpp:126-139`

OLD:
```cpp
void Bullet::onCollide(zenkit::MaterialGroup matId) {
  if(isFinished())
    return;
  if(matId != zenkit::MaterialGroup::NONE) {
    if(material < ItemMaterial::MAT_COUNT) {
      auto s = wrld->addLandHitEffect(ItemMaterial(material),matId,obj->matrix());
      s.play();
      }
    }
  Effect::onCollide(*wrld,vfx.handle(),obj->position(),nullptr,ow,spellId());
  vfx.setLooped(false);
  vfx.setPhysicsDisable();
  wrld->runEffect(std::move(vfx));
  }
```
NEW:
```cpp
bool Bullet::onCollide(zenkit::MaterialGroup matId) {
  if(isFinished())
    return true;

  // NOTE: in original-game oCVisualFX::ReportCollision @0x00494e80 the level-polygon
  // (zCCollObjectLevelPolys) branch ends the flying FX (ResetToOldMovementState ->
  // Collide @0x00493a00 -> EndEffect) only when emActionCollStat carries the COLLIDE bit
  // (this+0x3a0 & 1). A spell FX whose emActionCollStat is CREATE/BOUNCE/NORESP/CREATEQUAD
  // (no COLLIDE bit) passes through world geometry and keeps flying. OpenGothic always
  // stopped a spell on the first level-mesh hit. This mirrors the emActionCollDyn
  // pass-through handled in Bullet::onCollide(Npc&). Non-spell bullets keep stopping.
  const VisualFx* root = vfx.handle();
  const bool stop = !isSpell() || root==nullptr ||
                    (root->emActionCollStat & VisualFx::Collide)!=0;
  if(!stop)
    return false;

  if(matId != zenkit::MaterialGroup::NONE) {
    if(material < ItemMaterial::MAT_COUNT) {
      auto s = wrld->addLandHitEffect(ItemMaterial(material),matId,obj->matrix());
      s.play();
      }
    }
  Effect::onCollide(*wrld,vfx.handle(),obj->position(),nullptr,ow,spellId());
  vfx.setLooped(false);
  vfx.setPhysicsDisable();
  wrld->runEffect(std::move(vfx));
  return true;
  }
```

### Notes / scope

- Matches the deliberately minimal scope of the existing `emActionCollDyn` fix: only the
  stop/pass-through decision is corrected. The original's additional static-collision
  sub-behaviors (BOUNCE `ResetToOldMovementState` ricochet, `CREATEQUAD` decal, the
  per-FX CREATE-spawn throttle counter) are left as-is.
- Grep-verified symbols: `VisualFx::Collide` (`game/graphics/visualfx.h:20`),
  `VisualFx::emActionCollStat` (`:134`), `Bullet::isSpell()`, `Effect::handle()`,
  `Bullet::onCollide(Npc&)` precedent (`game/world/bullet.cpp:155-164`).
