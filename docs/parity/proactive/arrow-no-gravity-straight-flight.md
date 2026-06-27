# Arrow flight: original fires arrows in a straight line (gravity disabled); OpenGothic simulates a gravity arc

**Confidence:** Medium-High (original behavior is strongly evidenced; the fix is a coordinated multi-site change and is flagged below).

## Original function + address (prose only)

- `oCAIArrow::SetupAIVob` @ `0x006a10e0` (`oAiShoot.cpp`) is the arrow/bolt launch routine called from
  `oCNpc::DoShootArrow` @ `0x007446b0`. It computes the aim direction as the normalized vector from the
  weapon right-hand node (`ZS_RIGHTHAND` trafo) to the target's world position, calls
  `zCVob::SetHeadingAtWorld`, and sets the projectile's rigid-body velocity to that **unit direction times a
  constant launch speed** (a single `zVEC3 * float` multiply). There is no upward/ballistic term added to the
  velocity: the velocity points exactly at the target's collision center.
- Immediately after `zCRigidBody::SetVelocity`, SetupAIVob fetches the projectile's rigid body and clears
  bit 0 of the flag byte at body offset `0x100` (`flags &= 0xFE`).
- `zCRigidBody::Integrate` @ `0x005b5a50` (`zPhysics.cpp`) only adds the gravity acceleration
  (constant `0xC4754000` = `-981.0` cm/s² applied to the Y velocity component) when that bit-0 flag is set
  (`if ((flags & 1) == 0) goto <skip-gravity>`). With the bit cleared by SetupAIVob, **the arrow body gets no
  gravity** and integrates at constant velocity.
- `oCAIArrowBase::DoAI` @ `0x006a0640` (the per-tick arrow update) applies **no** gravity either — it only
  advances the trail strip, a fade timer, and the stuck-in-target shutdown; the flight integration is left
  entirely to the (gravity-disabled) rigid body.

Net original behavior: an arrow/bolt is launched straight at the target's collision center and flies in a
straight line at constant speed. There is no drop and no ballistic arc.

## OpenGothic file:line

- `game/world/world.cpp:671-704` — `World::shootBullet(...)` adds a ballistic launch compensation
  `dir.y += 0.5f*DynamicWorld::gravity*t` (twice: the target branch ~line 683 and the interactive branch ~694).
- `game/physics/dynamicworld.cpp:874-976` — `DynamicWorld::moveBullet(...)` integrates the projectile with
  gravity for non-spell bullets: position `to = pos + dir*dtF - Vec3(0,(isSpell?0:gravity*dtF*dtF),0)`
  (line 879) and velocity `d.y -= gravity*dtF` (lines 965-966).

## Divergence

OpenGothic applies `DynamicWorld::gravity` to non-spell bullets every tick, so arrows/bolts follow a parabola,
and `World::shootBullet` pre-tilts the launch velocity upward (`+0.5*g*t`) so the parabola still passes through
the target. The original fires arrows in a **straight line with rigid-body gravity disabled**.

Practical consequence: with `gravity = 9.8*100/(1000*1000) = 9.8e-4` cm/ms² and `bulletSpeed = 3` cm/ms, a
30 m (3000 cm) target-locked shot takes `t = 1000` ms and is launched with `+0.49` cm/ms of vertical velocity,
lobbing the arrow ~1.2 m above the line of sight before it falls back onto the target. The original arrow
travels dead straight along the line of sight. The end impact point matches only because OpenGothic's
compensation is tuned to land on the target; the **trajectory shape, flight visuals, and time-to-impact differ**,
and any shot without a locked target (free-aim / monster firing at a stale position) lands differently because
the original keeps the full 3-D weapon-node aim direction while OpenGothic's no-target branch
(`world.cpp:695-699`) forces a purely horizontal `dir` (`dir.y == 0`) and then lets gravity drop it.

## Proposed patch

This is a coordinated change across two files (you cannot disable gravity in `moveBullet` alone: with the
`+0.5*g*t` launch tilt still present and no gravity to pull the arrow back, arrows would sail upward forever).
Both edits must land together.

**`game/physics/dynamicworld.cpp` — `DynamicWorld::moveBullet`**

OLD (line 879):
```cpp
  auto  to  = pos + dir*dtF - Tempest::Vec3(0,(isSpell ? 0 : gravity*dtF*dtF),0);
```
NEW:
```cpp
  // NOTE: in original-game oCAIArrow::SetupAIVob @0x006a10e0 clears rigid-body gravity bit
  // (flags@0x100 & 0xFE) and oCAIArrowBase::DoAI @0x006a0640 adds no gravity, so arrows/bolts
  // fly in a straight line at constant velocity. zCRigidBody::Integrate @0x005b5a50 applies
  // gravity (-981 cm/s^2) only when that bit is set, which projectiles never have. Only spells
  // were already gravity-free here; arrows must be too.
  auto  to  = pos + dir*dtF;
```

OLD (lines 963-968):
```cpp
    const float l = b.speed();
    auto        d = b.direction();
    if(!isSpell)
      d.y -= (gravity)*dtF;
    b.move(to);
    b.setDirection(d);
```
NEW:
```cpp
    const float l = b.speed();
    auto        d = b.direction();
    // NOTE: see oCAIArrow::SetupAIVob @0x006a10e0 — arrow rigid bodies fly without gravity.
    b.move(to);
    b.setDirection(d);
```

**`game/world/world.cpp` — `World::shootBullet`** (remove the two ballistic-compensation tilts so the
launch velocity points straight at the target, matching `SetHeadingAtWorld` + constant-speed velocity).

OLD (target branch, ~lines 678-683):
```cpp
    float lxz   = std::sqrt(dir.x*dir.x+0*0+dir.z*dir.z);
    float speed = DynamicWorld::bulletSpeed;
    float t     = lxz/speed;

    dir/=t;
    dir.y += 0.5f*DynamicWorld::gravity*t;
```
NEW:
```cpp
    // NOTE: in original-game oCAIArrow::SetupAIVob @0x006a10e0 the launch velocity is the unit
    // direction toward the target's collision center times a constant speed, with no upward
    // gravity-compensation term (arrows fly straight). Normalize to bulletSpeed; no +0.5*g*t tilt.
    float len   = dir.length();
    if(len>0.f)
      dir *= (DynamicWorld::bulletSpeed/len);
```

OLD (interactive branch, ~lines 689-694):
```cpp
    float lxz   = std::sqrt(dir.x*dir.x+0*0+dir.z*dir.z);
    float speed = DynamicWorld::bulletSpeed;
    float t     = lxz/speed;

    dir/=t;
    dir.y += 0.5f*DynamicWorld::gravity*t;
```
NEW:
```cpp
    // NOTE: see oCAIArrow::SetupAIVob @0x006a10e0 — straight-line launch, no gravity tilt.
    float len   = dir.length();
    if(len>0.f)
      dir *= (DynamicWorld::bulletSpeed/len);
```

(`Tempest::Vec3::length()` is already used throughout OpenGothic, e.g. `dynamicworld.cpp:950` `(to-pos).length()`.)

### Caveat / why "Medium-High" and not "High"

The original-side evidence (gravity-disabled flag in SetupAIVob, no gravity in DoAI, `-981.0` gated on the
cleared bit in Integrate) is strong and mutually corroborating. The reason this is not graded "High" is that the
fix is a deliberate reversal of an existing OpenGothic design (the `+0.5*g*t` auto-compensation was clearly
intentional) and spans two files; it compiles cleanly (build-verifiable) but its full gameplay effect — straight
vs. arced arrows, and the no-target free-aim case — should be confirmed in-game before merging. If a single
surgical edit is required, treat this as **DEFERRED** pending in-engine validation, but the documented divergence
itself is high-confidence.
