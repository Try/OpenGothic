# Spell projectile never tracks (homes toward) its target after launch

**Confidence:** Medium-High (the code-level divergence is certain; player-visibility depends on which spell FX define a TARGET trajectory). Patch: **DEFERRED** (feature-scope, multi-layer) — reason below.

## Original function + address (prose only)
The original spell projectile is an `oCVisualFX` whose flight path is a key-framed
trajectory built by `oCVisualFX::CalcTrajectory` (Gothic2.exe @0x0048f620). The FX
update path `oCVisualFX::UpdateFXByEmitterKey` (@0x0048ddc0) re-invokes the virtual
CalcTrajectory (through the vtable slot at +0x128) on every emitter-key update, i.e.
every frame the FX is alive.

CalcTrajectory has two relevant phases:
- Build phase (the large branch that emits `oCTrajectory::InsertKey` calls): for a
  TARGET-style trajectory it lays out a curved spline from the origin vob toward the
  target, using the cached target position/node.
- Update phase (the early `if(*param_1 != 0)` branch): it fetches the target vob at
  `this+0x4b0` (and the resolved target node at `this+0x4a0`), reads that vob's current
  world transform, and rewrites the **last** trajectory key's matrix
  (`zCPositionKey::SetMat` on the final key) to the target's present world position,
  then flags the spline `Changed`. The projectile therefore continuously re-aims at the
  moving target — homing.

The target binding itself is established by `oCVisualFX::SetTarget` (@0x004912e0):
it stores the target vob at `this+0x4b0`, caches the target position at `this+0x4f4`,
and resolves `emTrjTargetNode` via `zCModel::SearchNode` into `this+0x4a0`.

## OpenGothic file:line
- `game/world/world.cpp:644` — `World::shootSpell(...)`
- `game/world/worldobjects.cpp:637` — `WorldObjects::shootBullet(...)` (sets `dir*speed/l` once)
- `game/physics/dynamicworld.cpp:874` — `DynamicWorld::moveBullet(...)` (advances `pos += dir*dt`, never re-aims)
- `game/physics/dynamicworld.h` — `BulletBody` stores only `dir/dirL/totalL/tgRange`; it holds **no** target `Npc*`.
- `game/world/objects/npc.cpp:3333` — `Npc::commitSpell()` even does `b.setTarget(nullptr)`, dropping the only target reference the bullet had.

## Divergence
In OpenGothic `World::shootSpell` computes the launch direction exactly once
(`dir = tgPos - pos`, normalized to `spellSpeed` in `shootBullet`). From then on the
projectile is a ballistic ray: `DynamicWorld::moveBullet` only does
`to = pos + dir*dtF` with a fixed `dir`, and `BulletBody` has no way to know its target.
The spell therefore flies in a perfectly straight line aimed at where the target's
collision-center was at the instant of casting.

The original re-runs `CalcTrajectory` each frame and slides the trajectory's terminal
key onto the target vob's *current* node position, so a TARGET-trajectory spell curves
to follow a moving/strafing enemy and connects even if the target has displaced since
launch. Against a moving target with a slow projectile (spellSpeed is a flat
`1 cm/ms` in OpenGothic, `dynamicworld.h:42`), OpenGothic's straight ray can miss where
the original would still hit.

## Proposed patch
**DEFERRED.** A faithful fix is not surgical: it requires (1) carrying the target `Npc*`
down into the physics-level `BulletBody` (which today is intentionally target-agnostic
and is even cleared at `npc.cpp:3333`), and (2) adding a per-tick re-aim/steer step in
`DynamicWorld::moveBullet` / the bullet `tick`. Both cross the game/physics boundary and
introduce new state and a turn-rate policy that cannot be derived 1:1 from the original
without reconstructing the `oCTrajectory` spline + per-key easing in `CalcTrajectory`
(~6 KB of curve math), so a small edit would be a guess rather than a parity match.
"Empty beats false positives": deferring until the trajectory model is ported.

Scope note for a future faithful implementation (so it is not re-derived from scratch):
the homing target is the vob/node cached by `oCVisualFX::SetTarget` (@0x004912e0, fields
`this+0x4b0`/`this+0x4a0`), and only the spline's **last** key is moved to the live
target each frame by `oCVisualFX::CalcTrajectory` (@0x0048f620) — the intermediate path
is not rebuilt — which bounds how aggressively the projectile curves.

// NOTE: in original-game oCVisualFX::CalcTrajectory @0x0048f620 (driven every frame via
// oCVisualFX::UpdateFXByEmitterKey @0x0048ddc0) the spell-FX trajectory's terminal key is
// re-set to the target vob's current world position (target bound by oCVisualFX::SetTarget
// @0x004912e0, fields +0x4b0/+0x4a0), so a TARGET-trajectory spell homes onto a moving
// target; OpenGothic computes a single fixed launch direction in World::shootSpell and
// never re-aims.
