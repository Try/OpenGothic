# Untargeted spell cast ignores the VFX origin node when choosing the spawn position

**Confidence:** Medium

## Original function + address (prose only)

`oCVisualFX::CalcTrajectory` @ `0x0048f620` (called from the cast/shoot path,
sibling of `oCVisualFX::CreateAndCastFX` @ `0x0048ee80`).

The original builds the projectile trajectory from two independent endpoints:

- The **origin** endpoint is resolved from the FX *origin vob* (`this+0x4a8`) and
  the *origin-node* index (`this+0x49c`). When an origin node is set it takes the
  hand/node world matrix via `zCVob::GetTrafoModelNodeToWorld`; otherwise it takes
  the origin vob's own trafo translation (`origin_vob + 0x3c`). This origin block
  runs **unconditionally** — it does not depend on whether a target vob is present.
- The **target** endpoint (`this+0x4b0` / target-node `this+0x4a0`) is only consulted
  for the far end of the trajectory / aim direction.

So the spawn position of a cast projectile is always the FX's resolved origin (origin
node when present, else the origin-vob position). Target presence only changes the
*direction* end of the trajectory, never the origin. The origin/target node strings
correspond to the FX-instance fields `emTrjOriginNode` / `emTrjTargetNode`.

## OpenGothic file:line

`game/world/world.cpp:644-669` (`World::shootSpell`).

```
650  if(target!=nullptr) {
651    auto tgPos = target->collosionCenter();
652    if(vfx!=nullptr && !vfx->emTrjOriginNode.empty()) {
653      pos = npc.mapBone(vfx->emTrjOriginNode);     // honors origin node
654      }
...
659    } else {
660    float a = npc.rotationRad();
661    dir.x = std::cos(a);
662    dir.z = std::sin(a);
663    pos  = npc.mapWeaponBone();                     // IGNORES emTrjOriginNode
664    }
```

## Divergence

OpenGothic resolves the projectile spawn origin **asymmetrically** depending on
whether a target exists:

- With a target (line 652-654) it honors `vfx->emTrjOriginNode`, falling back to
  the caster's `collosionCenter()` (which OpenGothic uses to model the ZenGin vob
  position) when the origin node is empty.
- Without a target (line 663) it discards `vfx->emTrjOriginNode` entirely and
  hardcodes `npc.mapWeaponBone()`.

The original resolves the origin endpoint identically regardless of target presence
— always origin-node-or-origin-vob. Consequently, any projectile spell whose VFX
defines an `emTrjOriginNode` different from the weapon bone spawns from the wrong
position when cast **without** a focus target (e.g. player free-cast with no NPC in
focus): OpenGothic spawns it at the weapon node instead of the configured origin
node. The targeted-cast path already does the right thing, so this is also an
internal inconsistency within `shootSpell`.

This is distinct from the already-filed `vfx-target-trajectory-wrong-node.md`
(which is about the FX *target* endpoint binding to the wrong bone) and from the
spell-damage / FX-invest / pass-through fixes.

## Proposed patch

Make the no-target branch honor `emTrjOriginNode` the same way the targeted branch
does, keeping `mapWeaponBone()` only as the empty-node fallback (lowest-risk: it
only changes untargeted casts of spells that declare an origin node, leaving the
common empty-node case untouched).

OLD (`game/world/world.cpp`):
```
    } else {
    float a = npc.rotationRad();
    dir.x = std::cos(a);
    dir.z = std::sin(a);
    pos  = npc.mapWeaponBone();
    }
```

NEW:
```
    } else {
    float a = npc.rotationRad();
    dir.x = std::cos(a);
    dir.z = std::sin(a);
    // NOTE: in original-game oCVisualFX::CalcTrajectory @0x0048f620 the projectile
    // origin endpoint is resolved from the FX origin node (emTrjOriginNode) / origin
    // vob independently of whether a target exists; only the far/aim end depends on
    // the target. Mirror the targeted branch's origin-node handling here instead of
    // unconditionally substituting the weapon bone.
    if(vfx!=nullptr && !vfx->emTrjOriginNode.empty())
      pos = npc.mapBone(vfx->emTrjOriginNode); else
      pos = npc.mapWeaponBone();
    }
```

Grep-verified OG symbols: `World::shootSpell` (`game/world/world.cpp:644`),
`VisualFx::emTrjOriginNode` (`game/graphics/visualfx.h:112`),
`Npc::mapBone` (`game/world/objects/npc.h:365`),
`Npc::mapWeaponBone` (`game/world/objects/npc.h:363`),
`Npc::collosionCenter` (`game/world/objects/npc.h:126`).

### Residual uncertainty (why Medium, not High)
Whether this manifests in practice depends on real G2 spell VFX configs: if a
projectile spell's `emTrjOriginNode` happens to coincide with the weapon bone, the
two origins agree and there is no visible change. The structural divergence (origin
resolution must be target-independent, and the no-target branch currently ignores
`emTrjOriginNode`) is firm; the magnitude per-spell is config-dependent. The patch
is conservative (empty-node behavior unchanged).
