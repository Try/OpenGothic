# Arrow landscape-hit: ricochet/stick material selection is inverted (only METAL/STONE bounce)

**Confidence:** Medium-High (divergence is certain; proposed patch carries moderate behavioral risk)

## Original fn + address
`oCAIArrowBase::ReportCollisionToAI` @ `0x006a09c0` (Gothic2.exe, `oAiShoot.cpp`).
When an arrow's `zCCollObjectProjectile` reports a collision against `zCCollObjectLevelPolys`,
the engine iterates the contact polygons and reads each polygon's material group
(`material+0x40`). It performs the special "embed/stick" response — disable physics and shove
the arrow 15.0 cm (`0x41700000`) along the inverse right-vector so it sits buried in the
surface — **only** when a contact polygon's material group equals **3 (WOOD)**. For every
other material group it takes the generic branch: it re-enables the rigid body's gravity bit
(`rigidbody[0x100] &= 0xfe; |= 1`) and scales the rigid-body velocity by **0.8**
(`0x3f4ccccd`), i.e. the arrow keeps bouncing/settling under gravity rather than stopping
dead. The same material-group==3 test gates the embed path for dynamic prog-mesh objects in
the second branch of the same function. (Material-group ids confirmed against
`zenkit::MaterialGroup`: METAL=1, STONE=2, WOOD=3, EARTH=4, WATER=5, SNOW=6.)

## OG file:line
`game/physics/dynamicworld.cpp:925-955` (`DynamicWorld::moveBullet`, non-spell landscape branch).

## Divergence
OpenGothic ricochets an arrow **only** when the hit material is `METAL` or `STONE`
(`matId==METAL || matId==STONE`), reflecting the direction and retaining `0.5` of the speed
(`dir*=(l*0.5f); //slow-down`). For **all other** materials it stops the arrow dead
(`stopBullet = true`).

The original does the opposite partition:
- It **sticks** (stops) the arrow only in **WOOD** (material group 3).
- It **bounces** the arrow off **everything else** — including EARTH/WATER/SNOW/UNDEF, not just
  METAL/STONE — retaining **0.8** of the velocity (not 0.5).

Net observable result: in OpenGothic an arrow that strikes earth/ground (the most common
landscape hit) stops instantly, whereas in the original it skids/settles via damped bounces;
and a metal/stone ricochet loses 50 % of its speed instead of 20 %. The WOOD case happens to
match (both stop), but the material set and the damping constant are both wrong.

## Proposed patch
Mirror the original partition: ricochet off any non-WOOD material, stick only in WOOD, and use
the 0.8 retention constant.

OLD (`game/physics/dynamicworld.cpp:925-936`):
```cpp
      if(callback.matId==zenkit::MaterialGroup::METAL ||
         callback.matId==zenkit::MaterialGroup::STONE) {
        auto d = b.dir;
        btVector3 m = {d.x,d.y,d.z};
        btVector3 n = callback.m_hitNormalWorld;

        n.normalize();
        const float l = b.speed();
        m/=l;

        btVector3 dir = m - 2*m.dot(n)*n;
        dir*=(l*0.5f); //slow-down
```

NEW:
```cpp
      // NOTE: in original-game oCAIArrowBase::ReportCollisionToAI @0x006a09c0 an arrow embeds
      // (stops) only when a contact landscape polygon's material group is WOOD(3); every other
      // material takes the generic bounce branch that re-enables gravity and scales the rigid-body
      // velocity by 0.8 (0x3f4ccccd). OpenGothic bounced only off METAL/STONE and stopped dead on
      // earth/ground, with a 0.5 (not 0.8) speed retention.
      if(callback.matId!=zenkit::MaterialGroup::WOOD) {
        auto d = b.dir;
        btVector3 m = {d.x,d.y,d.z};
        btVector3 n = callback.m_hitNormalWorld;

        n.normalize();
        const float l = b.speed();
        m/=l;

        btVector3 dir = m - 2*m.dot(n)*n;
        dir*=(l*0.8f); //slow-down (original retains 0.8 of velocity)
```

The `else` branch (lines 947-955) keeps the WOOD-only stick/stop behavior unchanged.

Risk note: OpenGothic's bounce model is a discrete reflection capped at `hitCount()>3` rather
than the original's continuous rigid-body settling, so widening ricochet to earth/water is a
behavioral change (arrows now skip a few times on the ground before the 3-hit cap stops them)
rather than a pure 1:1 reproduction. If only the lowest-risk change is desired, restrict the
patch to the constant `0.5f -> 0.8f` on line 936 and keep the METAL/STONE condition; that alone
is an unambiguous constant-parity fix.
