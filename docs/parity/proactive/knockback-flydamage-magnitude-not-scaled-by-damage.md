# FLY throwback impulse magnitude does not scale with inflicted damage

**Confidence:** High (divergence is certain); **patch DEFERRED** (no 1:1 unit mapping in OG's MoveAlgo model).

## Original function + address

`oCAIHuman::StartFlyDamage(float points, zVEC3& dir)` at `Gothic2.exe` `0x0069d940`, called
from `oCNpc::OnDamage_Anim` at `0x00679737`.

In the original, the FLY (knockback) throwback magnitude is *proportional to the damage dealt*:

- `OnDamage_Anim` passes `points` = the summed FLY damage of the blow, and a direction vector
  `dir = NormalizeApprox(victimPos - attackerPos)` (full 3D, away from the attacker; when there is
  no attacker vob it falls back to the victim's own world AtVector).
- `StartFlyDamage` reads three parser symbols — `DAMAGE_FLY_CM_PER_POINT`, `DAMAGE_FLY_CM_MIN`,
  `DAMAGE_FLY_CM_MAX` — and computes a centimetre throw distance:
  - `if(points <= 0) points = 1;`
  - `cm = clamp(DAMAGE_FLY_CM_PER_POINT * points, DAMAGE_FLY_CM_MIN, DAMAGE_FLY_CM_MAX);`
  - `magnitude = cm * 100.0f;`
  - horizontal impulse = `dir.xz * magnitude` (the passed-in `dir.y` is then *overwritten* by a
    fixed vertical launch `= aiHuman[+0x40] * 0.9f`, i.e. a constant upward component independent
    of the horizontal throw distance).
  - The result is applied to the rigid body via `zCRigidBody::ApplyImpulseCM(dir)`.

Net effect in the original: a heavy hit (troll/warg, high FLY damage) throws the victim much
farther than a light hit; the horizontal distance grows linearly with damage and is clamped to
`[CM_MIN, CM_MAX]`, while the vertical hop stays constant.

## OpenGothic file:line

`game/game/movealgo.cpp:500` — `MoveAlgo::accessDamFly(float dx, float dz, char hitType)`
(call site `game/world/objects/npc.cpp:2214`).

```
float len = std::sqrt(dx*dx+dz*dz);
auto  vec = Tempest::Vec3(dx,len*0.5f,dz);
vec = Tempest::Vec3::normalize(vec);
fallSpeed = vec*0.75f;
```

## Divergence

OpenGothic's throwback is a **fixed impulse of `0.75`** with a **fixed launch angle**
(`vec = (dx, len*0.5, dz)` normalized gives a constant ~26.6 degree arc, ~0.671 horizontal /
~0.335 vertical), entirely **independent of the damage value**. `accessDamFly` takes no damage
parameter (`game/game/movealgo.h:62`) and OpenGothic never references `DAMAGE_FLY_CM_PER_POINT`,
`DAMAGE_FLY_CM_MIN`, or `DAMAGE_FLY_CM_MAX` anywhere. Consequently a feeble fists/arrow FLY hit and
a maximum troll/warg FLY blow knock the victim back exactly the same distance — the original scales
that distance with `DAMAGE_FLY_CM_PER_POINT * points` clamped to `[CM_MIN, CM_MAX]`.

The damage value needed to fix this is available at the OG call site: `hitResult.value`
(`DamageCalculator::Val::value`, `game/game/damagecalculator.h:21`), computed at
`game/world/objects/npc.cpp` before the throwback block.

Also note (secondary, same root): the original's vertical launch is a constant
(`aiHuman[+0x40] * 0.9`), whereas OG ties the vertical component to the horizontal length
(`len*0.5`), so OG's arc shape differs as well.

## Proposed patch

**DEFERRED.** Threading `hitResult.value` into `accessDamFly` is signature-feasible, but a
high-confidence *parity* fix is not achievable surgically:

1. The original magnitude is a rigid-body impulse in centimetres (`cm * 100`,
   `ApplyImpulseCM`), integrated by ZenGin physics. OpenGothic replaces this with a `MoveAlgo`
   per-frame `fallSpeed` velocity (`npcFallSpeed = fallSpeed * dt`) whose base `0.75` was tuned to
   OG's own integrator. There is no documented/derivable conversion factor from the original's
   `cm`-impulse to OG's `fallSpeed` units, so any chosen coefficient would be tuning, not parity.
2. The clamp bounds and per-point rate live in parser symbols (`DAMAGE_FLY_CM_*`) that OpenGothic
   does not load; mapping them onto the `0.75` base requires an empirical coefficient.

Recommended follow-up (separate, tuning-validated change, not landed here): pass `hitResult.value`
to `accessDamFly`, read the three `DAMAGE_FLY_CM_*` parser ints, compute
`cm = clamp(CM_PER_POINT * max(points,1), CM_MIN, CM_MAX)`, and scale the horizontal impulse
proportionally to `cm` (relative to a reference at which `0.75` is correct), keeping the vertical
component constant. The *proportionality* and *clamp* are the portable parity-relevant behaviour;
only the absolute coefficient needs in-engine calibration.

```
// NOTE: in original-game oCAIHuman::StartFlyDamage (Gothic2.exe 0x0069d940), reached from
// oCNpc::OnDamage_Anim (0x00679737), the FLY throwback distance is
// clamp(DAMAGE_FLY_CM_PER_POINT*points, DAMAGE_FLY_CM_MIN, DAMAGE_FLY_CM_MAX) cm with a fixed
// vertical launch; OpenGothic uses a fixed 0.75 impulse independent of damage.
```
