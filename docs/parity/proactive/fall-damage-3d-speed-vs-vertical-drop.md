# Fall damage scales with 3D speed instead of vertical drop height

**Confidence:** High

## Original function + address

`oCNpc::CreateFallDamage(float)` at `Gothic2.exe 0x00681da0` computes fall damage as
roughly `(fallHeight + 50 - falldown_height) * 0.01 * falldown_damage`, where the
`fallHeight` argument is a **scalar vertical drop distance**. The caller
`oCAniCtrl_Human::CheckFallStates` at `0x006b5810` passes the ani-controller's tracked
fall-height field (the accumulated downward Y drop between takeoff and landing, a field
that sits right after the separately-stored horizontal launch-velocity vector). The
horizontal component of the character's motion never enters the damage calculation in the
original engine; only the vertical fall distance does.

## OpenGothic file:line

- `game/world/objects/npc.cpp:2210` — `Npc::takeFallDamage`
- `game/game/damagecalculator.cpp:42-48` — `DamageCalculator::damageFall`

## Divergence

`DamageCalculator::damageFall(Npc&, float speed)` reconstructs the fall height from an
impact velocity via kinematics: `fallTime = speed/gravity;
height = 0.5*|gravity|*fallTime^2` (i.e. `height = speed^2 / (2*|gravity|)`). This is the
correct inverse of free-fall **only when `speed` is the vertical impact velocity**
(`speed = sqrt(2*g*h)`).

But `Npc::takeFallDamage` feeds it `fallSpeed.length()` — the **full 3D speed magnitude**,
including the horizontal `x`/`z` components. `MoveAlgo::fallSpeed` carries real horizontal
velocity at the moment of landing in several paths: jump-forward accumulates `dp` into
`fallSpeed` (`movealgo.cpp:262`), slides add a tangential slope vector
(`movealgo.cpp:396`), wall/gravity bounces reflect the velocity off the surface normal
(`movealgo.cpp:1025-1031`), and the DamFly knockback sets a slanted vector
(`movealgo.cpp:512`). In all those cases `|fallSpeed| > |fallSpeed.y|`, so the
reconstructed `height` (and therefore the damage and the no-damage threshold crossing) is
inflated relative to the original, which only ever counts vertical drop. Jump-down
attacks, running leaps off ledges, and slope-falls thus take more fall damage in OpenGothic
than in `Gothic2.exe`.

This also contradicts OpenGothic's own fall-animation selection, which already uses the
vertical component only: `movealgo.cpp:362` computes `fallTime = fallSpeed.y/gravity` to
pick FallDeep vs Fall. The damage path at `npc.cpp:2210` is the odd one out.

## Proposed patch

Pass the vertical-component magnitude, matching both the original's vertical-drop semantics
and OpenGothic's own animation logic. `damageFall`'s `speed -> height` kinematics are
unchanged; only the scalar handed to it changes.

`game/world/objects/npc.cpp`

OLD:
```cpp
  auto dmg = DamageCalculator::damageFall(*this,fallSpeed.length());
```

NEW:
```cpp
  // NOTE: in original-game oCNpc::CreateFallDamage (Gothic2.exe 0x00681da0) scales damage
  // by the vertical fall-drop height only (the scalar passed by oCAniCtrl_Human::CheckFallStates
  // @0x006b5810), never by horizontal speed. damageFall() reconstructs height = speed^2/(2g),
  // which is the vertical drop only when fed the vertical impact velocity; fallSpeed.length()
  // adds horizontal jump/slide/bounce velocity and over-counts damage. Use the vertical
  // component, consistent with the FallDeep animation test in MoveAlgo (fallSpeed.y/gravity).
  auto dmg = DamageCalculator::damageFall(*this,std::abs(fallSpeed.y));
```

`std::abs` is already used in this file (e.g. `npc.cpp:1427`, `npc.cpp:3682`); `fallSpeed`
is a `const Vec3&` so `fallSpeed.y` is available; `damageFall` already treats its argument
as a non-negative speed.
