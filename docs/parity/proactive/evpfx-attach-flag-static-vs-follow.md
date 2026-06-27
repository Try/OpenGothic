# eventPFX `ATTACH` flag: static world-anchor vs. node-following

**Confidence:** Medium-High (divergence is certain; corrective direction verified against decompile but counterintuitive — recommend a runtime A/B before committing the code change)

## Original function + address (prose)

The particle-effect spawn for the `*eventPFX` tag (event type 6) is dispatched in
`zCModel::DoAniEvents` @ `0x0057b890` (Gothic2.exe). In the `case 6:` block the engine
reads the event's flag word at offset `+0x70` and branches on whether the `ATTACH`
keyword was present:

- The MDS parser `zCModelPrototype::ReadAniEnum` @ `0x00596c00` writes this flag while
  emitting the eventPFX chunk: it does `Search(rest, "ATTACH")` and stores
  `event+0x70 = (found ? 1.0f : 0.0f)` (the same offset later read by DoAniEvents).
  The handle/index is the float at `event+0x6c`; the pfx name and the node ("position")
  string are the two strings.

- In `DoAniEvents`, the branch is `if (event+0x70 != 0.0f)` (the Ghidra
  `(NAN(x)||NAN(0)) != (x==0)` idiom = `x != 0`):
  - **ATTACH present (`+0x70 != 0`):** compute `GetTrafoNodeToModel(node)` × model world
    trafo, `zCVob::SetTrafoObjToWorld(pfxVob, …)`, then `zCWorld::AddVob(world, pfxVob)`.
    The vob is a free-standing world object placed **once** at the node's world position;
    it is **not** entered into the model's `zTMdl_NodeVobAttachment` array, so the model
    never updates it again → it stays anchored in world space.
  - **ATTACH absent (`+0x70 == 0`):** `zCWorld::AddVobAsChild(...)` parents the pfx vob to
    the model host and `InsertEnd`s a `zTMdl_NodeVobAttachment{vob, nodeIdx}` so the model
    refreshes its node-relative trafo every frame → the effect **follows the node**.
  - If the named node is not found, it falls through to the static path at the model origin.

So in the original engine: **no `ATTACH` ⇒ follows the node; `ATTACH` ⇒ static, anchored
to the world position where it spawned.** (The keyword name is counterintuitive but the
two distinct code paths — attachment array vs. free world vob — are unambiguous.)

## OG file:line

- `game/graphics/mesh/animation.cpp:418` — `Animation::Sequence::processPfx(const MdsParticleEffect& p, …)`
  builds `Effect e(PfxEmitter(world,p.name), p.position)` and calls
  `visual.startEffect(world, std::move(e), p.index, false)`. **`p.attached` is never read.**
- `game/graphics/mdlvisual.cpp:878-881` — `MdlVisual::syncAttaches()` calls
  `i.view.setObjMatrix(pos)` for **every** effect each frame.
- `game/graphics/effect.cpp:158-212` / `294-307` — `bindAttaches` resolves `boneId` from the
  node name and `syncAttachesSingle`/`setObjMatrix` re-reads `pose->bone(boneId)` every frame.
- Confirmed unused: `grep -rn "attached" game/graphics/` yields no consumer of the
  `MdsParticleEffect::attached` field.

## Divergence

OpenGothic treats **every** eventPFX as node-following: the effect is bound to the node's
bone and its world matrix is re-synced from the pose on every frame (`syncAttaches`),
regardless of the `ATTACH` flag. This matches the original only for the **no-ATTACH**
(default) case. For an `ATTACH`-flagged eventPFX the original spawns a **static**, world-
anchored effect that does **not** move with the node, whereas OpenGothic keeps it glued to
the bone and drags it along with the animation. Visible on any `*eventPFX(... ATTACH)` whose
node moves during the effect's lifetime (the effect should stay where it was spawned).

## Proposed patch — DEFERRED

**DEFERRED reason:** A faithful fix is not surgical. `Effect` has no "freeze world matrix"
mode for the pfx node — `syncAttachesSingle`/`setObjMatrix` unconditionally recompute the
transform from `pose->bone(boneId)` every frame, and `MdlVisual::syncAttaches` iterates all
effects unconditionally. Implementing the static (ATTACH) path requires capturing the node's
world matrix once at spawn and excluding that effect from per-frame re-sync, which touches
the shared `Effect`/`MdlVisual` attach machinery and cannot be made build- and behavior-safe
in a single localized edit. Additionally, the corrective direction (`ATTACH` ⇒ static) is
counterintuitive relative to the keyword name and the modding-community lore, so it warrants
an in-game A/B verification before being committed.

Sketch of the intended change (not applied):

```cpp
// game/graphics/mesh/animation.cpp  Animation::Sequence::processPfx(const MdsParticleEffect& p, ...)
// NOTE: in original-game zCModel::DoAniEvents @0x0057b890, eventPFX with the ATTACH flag
//       (event+0x70!=0, parsed in zCModelPrototype::ReadAniEnum @0x00596c00) is spawned
//       as a free world vob at the node's spawn-time world trafo (AddVob, no
//       zTMdl_NodeVobAttachment entry) and does NOT follow the node; only non-ATTACH
//       eventPFX are parented to the node (AddVobAsChild + node-vob attachment) and follow.
else if(!p.name.empty()) {
  Effect e(PfxEmitter(world,p.name), p.position);
  e.setActive(true);
  if(p.attached) {
    // static: capture node world matrix once, mark effect as world-anchored (no per-frame re-sync)
    e.setWorldAnchored(true);              // new Effect mode to add
  }
  visual.startEffect(world, std::move(e), p.index, false);
}
```

The `setWorldAnchored` mode (and a corresponding guard in `MdlVisual::syncAttaches` /
`Effect::setObjMatrix` so anchored pfx keep their spawn-time bone matrix) is the prerequisite
infra work that makes this DEFERRED rather than a one-line fix.
