# Look-At head-turn neutral-snap gate is 80° in OpenGothic vs 90° in the original

**Confidence:** High (for the horizontal gate threshold); Y-clamp divergence noted but DEFERRED.

## Original function + address

The head-look-at target is set by `oCAniCtrl_Human::SetLookAtTarget` (the `zVEC3&`
overload @ `0x006b6360`, and the `zCVob*` overload @ `0x006b6490`). Both call
`oCNpc::GetAngles` (@ `0x006812b0`) to obtain the horizontal angle (azimuth) and the
vertical angle (elevation), in degrees, between the NPC's facing/at-vector and the
look-at target. They then drive the head controller with two **normalized [0,1]**
values where `0.5` is the neutral/centered head pose:

- The azimuth is first truncated to an integer and tested `abs(int(azimuth)) < 90`.
  If the target is **90° or more** to the side, the head is snapped to neutral:
  both controller values are set to `0.5` (the `else` branch loads `0.5`/`0.5`).
- Below 90°, the controller values are computed as
  - horizontal = clamp(azimuth/180 + 0.5, 0, 1)  → azimuth effectively limited to ±90°,
  - vertical   = clamp(0.5 − elevation/120, 0, 1) → elevation effectively limited to ±60°.

The load-bearing piece of *logic* (independent of the controller's bone-degree
mapping) is the gate: the head returns to neutral once the horizontal angle to the
target reaches **90°**.

## OpenGothic file:line

`game/world/objects/npc.cpp:1400-1438` (`Npc::implLookAt`), specifically:
- line 1402: `static const float maxRot = 80; // maximum rotation`
- lines 1414-1417: when `dst.x < -maxRot || dst.x > maxRot`, set `dst = (0,0)` (neutral).

`dst.x` here is the same quantity as the original's azimuth: it is
`visual.viewDirection() - angleDir(dx,dz)` normalized to [-180,180], i.e. the
horizontal angle in degrees between facing and the target. It is applied directly as
head-bone degrees via `Pose::setHeadRotation` → `mkSkeleton` (pose.cpp:455-458), not
through a normalized controller.

## Divergence

The original snaps the head back to neutral when the horizontal angle to the look-at
target reaches **90°**; OpenGothic does so at **80°** (`maxRot = 80`). Because in
OpenGothic `dst.x` is not separately clamped, `maxRot` simultaneously bounds the
maximum horizontal head rotation, so it is the analogue of the original's combined
±90° gate-and-clamp. Net effect: in OpenGothic an NPC stops tracking a target with its
head 10° earlier than in the original, and the maximum head-turn cone is 10° narrower.

Note the original's azimuth gate is a strict `< 90` on the integer-truncated value, so
the snap engages at azimuth ∈ [89,90) only once truncation yields 90 — practically the
threshold is 90°.

## Proposed patch

```cpp
// game/world/objects/npc.cpp  (Npc::implLookAt)
// OLD
  static const float rotSpeed = 200; // deg per second
  static const float maxRot   = 80; // maximum rotation
// NEW
  static const float rotSpeed = 200; // deg per second
  // NOTE: in original-game oCAniCtrl_Human::SetLookAtTarget @0x006b6360 the head
  // snaps to neutral once the horizontal angle to the target reaches 90deg
  // (abs(int(azimuth)) < 90), and the head-controller azimuth is mapped over +-90deg.
  static const float maxRot   = 90; // maximum rotation
```

This is a one-line, build-verifiable change of the horizontal gate/clamp from 80° to
90° to match the original's `< 90` azimuth gate and ±90° controller mapping.

### DEFERRED: vertical clamp ±20° vs original ±60°

OpenGothic clamps `dst.y` to ±20° (npc.cpp:1419-1422) and feeds it directly as head-bone
degrees. The original's vertical maps elevation over ±60° into a normalized [0,1]
head-controller value whose actual bone-degree output depends on the model's head-node
rotation limits (`zCModel`/head-controller apply path, not decoded here). Because the
two pipelines are not in the same units (direct bone degrees vs normalized controller),
±20° degrees cannot be shown equal-or-not to the original's ±60° normalized range with
high confidence. Deferred pending decode of the head-controller→bone degree mapping.
