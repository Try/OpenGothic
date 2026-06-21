# VFX TARGET-trajectory attaches to origin node instead of target node

**Confidence:** Medium-High

## Original function + address

`oCVisualFX::CalcTrajectory` @ `0x0048f620` and `oCVisualFX::Init` @ `0x00491f20`.

The original keeps two *distinct* model-node lookups on the FX:

- The **origin** endpoint of the trajectory resolves the node named by the
  emitter's *origin-node* string against the **origin** vob's model
  (the model at `this+0x4a8`), caching the resolved node at `this+0x49c`.
- The **target** endpoint resolves the node named by the emitter's
  *target-node* string against the **target** vob's model
  (the model at `this+0x4b0`), caching the resolved node at `this+0x4a0`.

In `CalcTrajectory`, the start matrix is taken from the origin model/origin-node
(`this+0x49c`/`this+0x4a8`); the end of the trajectory is taken from the target
model/target-node (`this+0x4a0`/`this+0x4b0`). I.e. the trajectory's *target*
end is positioned on the **target node**, never the origin node. These two node
strings correspond to the FX-instance fields `emTrjOriginNode` and
`emTrjTargetNode`.

## OpenGothic file:line

`game/graphics/effect.cpp:24` and `game/graphics/effect.cpp:178-182`.

```
24   nodeSlot  = root->emTrjOriginNode;
...
178  if((emTrjMode & VisualFx::Trajectory::Target)==VisualFx::Trajectory::Target && target!=nullptr) {
179    // NOTE: needed for shrink-spell, light-spell
180    p.identity();
181    p.translate(target->mapBone(nodeSlot));
182    }
```

## Divergence

When `emTrjMode` carries the `TARGET` bit (FX wraps the spell target — e.g.
shrink-spell, light-spell), OpenGothic positions the FX on the **target NPC**
but resolves the bone via `nodeSlot`, which was initialized from
`root->emTrjOriginNode` (effect.cpp:24). The original resolves the *target*
endpoint against the target's node named by `emTrjTargetNode`, not the
origin-node. So for any FX whose `emTrjTargetNode` differs from its
`emTrjOriginNode` (or where only the target node is specified), OpenGothic
attaches the target-bound FX to the wrong bone — falling back to the target's
body center via `mapBone`'s not-found path when the origin-node name does not
exist on the target skeleton. `emTrjTargetNode` is loaded
(`game/graphics/visualfx.cpp:67`, field `game/graphics/visualfx.h:113`) but is
otherwise never read anywhere in the codebase.

`mapBone` (`game/world/objects/npc.cpp:3578`) already falls back to body center
when the node name is empty/unknown, so the change is safe when
`emTrjTargetNode` is empty.

## Proposed patch

```
OLD (game/graphics/effect.cpp:178-182):
  if((emTrjMode & VisualFx::Trajectory::Target)==VisualFx::Trajectory::Target && target!=nullptr) {
    // NOTE: needed for shrink-spell, light-spell
    p.identity();
    p.translate(target->mapBone(nodeSlot));
    }

NEW:
  if((emTrjMode & VisualFx::Trajectory::Target)==VisualFx::Trajectory::Target && target!=nullptr) {
    // NOTE: needed for shrink-spell, light-spell
    // NOTE: in original-game oCVisualFX::CalcTrajectory @0x0048f620 the TARGET
    // endpoint is resolved against the target model's emTrjTargetNode
    // (cached at this+0x4a0/this+0x4b0), not the origin node.
    std::string_view tgtNode = (root!=nullptr && !root->emTrjTargetNode.empty())
                                 ? std::string_view(root->emTrjTargetNode)
                                 : nodeSlot;
    p.identity();
    p.translate(target->mapBone(tgtNode));
    }
```

Grep-verified OG symbols: `VisualFx::emTrjTargetNode` (visualfx.h:113, set at
visualfx.cpp:67), `Effect::root` / `Effect::nodeSlot` (effect.h), `Npc::mapBone`
(npc.cpp:3578).
