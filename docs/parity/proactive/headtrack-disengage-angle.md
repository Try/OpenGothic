# Head-tracking disengage angle: OG clamps at 80°, original at 90°

**Confidence:** Medium

## Original function + address

`oCAniCtrl_Human::SetLookAtTarget` exists in two overloads in the original
`Gothic2.exe`: the `zVEC3&` overload at **0x006b6360** and the `zCVob*`
overload at **0x006b6490** (both share the same look-at math). They compute the
off-axis angles to the look-at target via `oCNpc::GetAngles` (which fills a
horizontal/azimuth angle and a vertical/elevation angle), then:

- Convert the **horizontal off-axis angle** to an int and test
  `abs(angle) < 0x5a` — i.e. **90 degrees**. If the target is 90° or more off to
  the side, the head returns to the neutral center pose (both combine factors set
  to 0.5) and tracking effectively disengages.
- Otherwise it maps the horizontal angle to a combine-X blend factor as
  `angle * (1/180) + 0.5`, clamped to `[0,1]` — so the horizontal range is
  exactly `±90°` (90/180 = 0.5, +0.5 → 1.0; the gate and the blend range agree,
  which confirms the 0x5a test is on the horizontal angle).
- The vertical angle maps to `1.0 - (angle * (1/120) + 0.5)`, clamped to `[0,1]`
  — a `±60°` blend range.

The per-frame interpolation toward those factors lives in `FUN_006b6170`
(`oCAniCtrl_Human::LookAtTarget`, called via 0x006b62f0), which blends the
S_LOOKAT combine animation rather than rotating a bone directly.

(The original drives the head via a combine *animation* blend, so the vertical
`±60°` / horizontal `±90°` figures are blend-range limits, not literal bone
degrees. Only the **disengage gate** — "at what off-axis angle does the head stop
tracking and return to neutral" — is representation-independent and therefore
directly comparable to OpenGothic. This finding is limited to that gate.)

## OpenGothic file:line

`/Users/admin/Downloads/opengothic/game/world/objects/npc.cpp:1400` —
`Npc::implLookAt(...)`. The horizontal disengage gate is at line **1402 / 1414**:

```
1402   static const float maxRot   = 80; // maximum rotation
...
1414   if(dst.x<-maxRot || dst.x>maxRot) {
1415     dst.x = 0;
1416     dst.y = 0;
1417     }
```

`dst.x` is the horizontal off-axis angle (`visual.viewDirection() -
angleDir(dx,dz)`, normalized to `[-180,180]`). When it exceeds `maxRot`, OG zeroes
the target rotation, returning the head to neutral — the same behavior as the
original's `abs(angle) < 90` gate, but the threshold is **80° instead of 90°**.

The resulting `headRotX/headRotY` are applied directly to the `BIP01_HEAD` bone
in `game/graphics/mesh/pose.cpp:455-458`; there is no additional clamp downstream,
so `maxRot` is the sole horizontal disengage gate.

## Divergence

In the original, an NPC keeps its head turned toward a look-at target until the
target is a full **90°** off the NPC's facing direction; beyond that the head
snaps back to neutral. OpenGothic disengages **10° earlier, at 80°**, so NPCs
stop following targets/the player with their head sooner than vanilla when the
target is far to the side.

## Proposed patch

OLD (`game/world/objects/npc.cpp:1402`):
```cpp
  static const float maxRot   = 80; // maximum rotation
```
NEW:
```cpp
  // NOTE: in original-game oCAniCtrl_Human::SetLookAtTarget @0x006b6360 / @0x006b6490
  // the head-track disengage gate is abs(horizontal off-axis angle) < 90deg (0x5a);
  // beyond it the look-at combine factors snap to the neutral 0.5/0.5 pose.
  static const float maxRot   = 90; // maximum rotation (disengage gate)
```

Grep-verified OG symbols used by this path and untouched by the patch:
`Npc::implLookAt` (npc.cpp:1400), `maxRot` (npc.cpp:1402,1414),
`MdlVisual::viewDirection` (mdlvisual.cpp:795), `MdlVisual::headRotation`
(mdlvisual.cpp:444), `MdlVisual::setHeadRotation` (mdlvisual.cpp:439),
`Pose::setHeadRotation` applying to `BIP01_HEAD` (pose.cpp:455-458).

Scope note: only the horizontal disengage gate is changed. The vertical clamp
(`±20°`, npc.cpp:1419-1422) is intentionally left alone — the original's `±60°`
figure is a combine-animation blend range, not literal bone degrees, so it is not
a like-for-like comparison and changing it could over-rotate the head bone.
