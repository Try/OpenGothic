# Issue #903 — Jarkendar polypodiums (ferns) have collision; vanilla lets you walk through

**Disposition:** DEFER (collision-flag interpretation bug; needs the ZEN asset + runtime test)

The repo owner already noted on the issue: "I would need to chase collision flags, likely
OpenGothic not interpreting some bits in a right way."

## OG files
- `game/graphics/objvisual.cpp` — `ObjVisual::setVisual(const zenkit::VirtualObject&, ...)`
  (objvisual.cpp:128-184), specifically the collision decision at lines 131, 153-156 (MESH)
  and 174-182 (MODEL/MORPH_MESH)
- `game/world/objects/staticobj.cpp` — `StaticObj::StaticObj` (calls `visual.setVisual`)

## Original-game behavior (prose)
In Gothic2.exe, static world collision (player vs. world vobs) is governed by the vob's
*static* collision flag, and wind-animated decoration meshes are excluded from blocking the
player. Vegetation such as ferns is set up so the player walks through it freely; only
trees/palms (solid, non-wind static collision) block movement. Relevant flags: `zCVob`
`SetCollDetStat` (0x0061ce50) vs `SetCollDetDyn` (0x0061cf40) — the static flag drives world
collision, and wind/visual-animated vobs are not registered as solid static colliders.
// NOTE: in original-game wind-animated vegetation (e.g. polypodiums) does not participate
// in static player collision; only solid static vobs (trees/palms) block movement.

## OG current behavior / divergence
`ObjVisual::setVisual` derives collision solely from `cd_dynamic`:

```
131  const bool enableCollision = vob.cd_dynamic;    // collide with player
...
153  const bool windy = (vob.anim_mode!=zenkit::AnimationType::NONE && vob.anim_strength>0);
154  if(vob.show_visual && enableCollision && !windy) {
155    mesh.physic = PhysicMesh(*view,*world.physic(),false);   // MESH path
156    }
...
179  if(vob.show_visual && enableCollision) {                   // MODEL / MORPH_MESH path
180    mdl.physic = PhysicMesh(*view,*world.physic(),true);     // NO windy guard
181    mdl.physic.setSkeleton(view->skeleton.get());
182    }
```

Two candidate divergences, both consistent with the report:
1. The MESH path suppresses collision for `windy` vobs (153-154), but the MODEL/MORPH_MESH
   path (179) has **no** windy guard. If the polypodium uses a `.MMS`/`.MDL`/`.ASC` visual
   with `anim_mode != NONE`, it gets a collision body in OpenGothic that vanilla omits.
2. OpenGothic keys collision on `cd_dynamic` only, whereas the original keys *world* (static)
   collision on the static flag; a fern with `cd_static`/`cd_dynamic` differing from
   OpenGothic's single-flag assumption will collide incorrectly.

## Why DEFER (implementation guide)
Resolving requires the actual Jarkendar polypodium vob to read its visual type, `anim_mode`,
`anim_strength`, `cd_static` and `cd_dynamic` (not available here). Likely fix once
confirmed: hoist the `windy` guard from the MESH branch into a shared check applied to the
MODEL/MORPH_MESH branch too (objvisual.cpp:179), i.e.

  `if(vob.show_visual && enableCollision && !windy) { ... }`

and/or correct the static-vs-dynamic flag interpretation. Both touch collision for *all*
static vobs (trees, gates, props), so they must be validated against the issue save plus a
regression sweep of solid static objects → DEFER. The MODEL-path missing-`windy`-guard
asymmetry vs the MESH path is the strongest lead.
