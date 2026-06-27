# Scale parity: anim-derived root motion ignores NPC model_scale

**Confidence:** Medium-High

## Original function + address (prose only)
- `zCModel::SetModelScale` (Gothic2.exe @ `0x0057dc30`) stores the per-axis model scale
  and raises a "scaled" flag bit (`0x10`) in the model status byte whenever the scale is not
  exactly `(1,1,1)`.
- `zCModel::GetTrafoNodeToModel` (Gothic2.exe @ `0x0057a9c0`) builds the node-to-model
  transform by walking the node hierarchy, and — *only when the scaled flag is set* — post-
  multiplies the accumulated trafo by `Alg_Scaling3D(modelScale)` and then re-orthonormalizes.
  Because re-orthonormalization renormalizes the basis vectors but leaves the translation
  magnitude scaled, every node *position* reported in model space (including the root/movement
  node from which per-frame NPC displacement is derived) is multiplied by `model_scale`.
  Net effect in the original: a scaled-up NPC covers proportionally more ground per animation
  frame (and a scaled-down NPC less), i.e. anim-driven locomotion speed tracks `model_scale`.
- `oCNpc::SetModelScale` (Gothic2.exe @ `0x0072d7b0`) is the path reached from the
  `Mdl_SetModelScale` external; it forwards the vector to `zCModel::SetModelScale`.

## OpenGothic file:line
- `game/world/objects/npc.cpp:882` — `Npc::animMoveSpeed(...)` returns the pose root-motion
  delta verbatim, in model-local space, with no `model_scale` applied.
- `game/world/objects/npc.cpp:5086` — `sz[]` (the model scale) is applied **only** to the
  render/object matrix (`mt.scale(sz[0],sz[1],sz[2])`).
- `game/physics/dynamicworld.cpp:1163` — `NpcItem::setScale` is an explicit `// NOP for now`.

## Divergence
In OpenGothic `model_scale` is consumed exclusively by the visual object matrix. The
animation-derived locomotion delta returned by `Npc::animMoveSpeed` (consumed by
`MoveAlgo::animMoveSpeed`/`npcMoveSpeed` and applied to the NPC world position) is taken in
model-local space and is **not** multiplied by `sz`. In the original engine that same delta is
derived from node positions that `zCModel::GetTrafoNodeToModel` has already scaled by
`model_scale` (in model-local space, before the model-to-world rotation). Consequently a
scaled NPC in OpenGothic plays its walk/run cycle stretched visually but still translates at
base (unscaled) speed, whereas the original translates at `model_scale * base` speed. Default
NPCs (`sz == {1,1,1}`) are unaffected; the divergence only manifests on NPCs that received
`Npc_SetModelScale`/`Mdl_SetModelScale`.

The scaling order matches: the original scales the delta in model-local space *before* the
model-to-world rotation, and OpenGothic's `MoveAlgo::animMoveSpeed` calls `applyRotation` on
the value returned by `Npc::animMoveSpeed`, so scaling the delta componentwise inside
`Npc::animMoveSpeed` (before rotation) reproduces the original sequencing. For the typical
uniform model scale the per-axis vs. uniform distinction is immaterial.

## Proposed patch
File: `game/world/objects/npc.cpp`

OLD:
```cpp
Tempest::Vec3 Npc::animMoveSpeed(uint64_t dt) const {
  return visual.pose().animMoveSpeed(owner.tickCount(),dt);
  }
```

NEW:
```cpp
Tempest::Vec3 Npc::animMoveSpeed(uint64_t dt) const {
  // NOTE: in original-game zCModel::GetTrafoNodeToModel @0x0057a9c0 post-multiplies the
  // node-to-model trafo by Alg_Scaling3D(model_scale) when the scaled flag set by
  // zCModel::SetModelScale @0x0057dc30 is active, so anim-derived root motion is scaled by
  // model_scale (in model-local space, before the model-to-world rotation). OpenGothic applied
  // sz only to the render matrix, leaving scaled NPCs locomoting at base speed.
  auto dp = visual.pose().animMoveSpeed(owner.tickCount(),dt);
  dp.x *= sz[0];
  dp.y *= sz[1];
  dp.z *= sz[2];
  return dp;
  }
```

Grep-verified OG symbols: `Npc::animMoveSpeed` (`game/world/objects/npc.h:185`,
`game/world/objects/npc.cpp:882`); `sz` member (`game/world/objects/npc.h:550`,
`float sz[3]={1.f,1.f,1.f}`); `owner` member used in the same function; `Pose::animMoveSpeed`
(`game/graphics/mesh/pose.cpp:605`); consumer `MoveAlgo::animMoveSpeed`
(`game/game/movealgo.cpp:549`) applies `applyRotation` after this call.

### Related (DEFERRED, separate fix)
`DynamicWorld::NpcItem::setScale` is an explicit NOP (`dynamicworld.cpp:1163`), so the
collision capsule, bbox, reach/`translateY` height etc. also ignore `model_scale`. The
original rebuilds the scaled model bbox (`zCModel::SetModelScale` recomputes the bbox via
`Alg_Prod(scale, bbox)`). DEFERRED: that requires rebuilding/refitting the Bullet capsule and
touching multiple bbox/height consumers — not a surgical one-liner and out of scope for this
high-confidence change.
