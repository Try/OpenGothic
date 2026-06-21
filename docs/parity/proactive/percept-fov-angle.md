# Perception: horizontal sight FOV half-angle (91 deg vs 80 deg)

Confidence: Medium

## Original fn + addr
`oCNpc::CanSee` (0x00741c10), using `oCNpc::GetAngles` (0x006812b0).

After a successful line-of-sight ray, the original (non-freeLos path) computes the
horizontal angle (azimuth) between the NPC's facing direction and the direction to
the target via `GetAngles`, in degrees, normalized to [-180,180] with 0 = target
directly ahead. It then accepts the target as "seen" only when the absolute value of
the truncated azimuth is strictly less than `0x5b` = 91. So the original's horizontal
sight cone is +-91 degrees about the forward direction (a 182-degree total arc).

## OG file:line
`game/world/objects/npc.cpp:4684` and `4691-4695` (`Npc::canRayHitPoint`).

OG uses `static const double ref = cos(100*pi/180)` and accepts when
`cos(da) <= ref`, where `da = viewDirection - angleDir(self.x-pos.x, self.z-pos.z)`.
Because `dx/dz` point from the target back to the observer, a target dead-ahead gives
`da ~= 180 deg` (`cos = -1`), and the boundary `cos(da) = cos(100 deg)` occurs at
`da = 100 deg`, i.e. a forward offset of `180 - 100 = 80 deg`. So OG's effective
horizontal sight cone is +-80 degrees about forward (160-degree total arc), despite
the comment reading "+-100 view angle range".

## Divergence
Original horizontal sight half-angle = 91 deg; OG effective half-angle = 80 deg.
OG NPCs have an 11-degree narrower cone on each side. Gameplay-different: targets in
the 80..91 degree peripheral band that the original would spot are invisible to OG
NPCs (easier to sneak past guards / flank without being seen).

## Proposed patch
```cpp
// OLD (game/world/objects/npc.cpp)
  static const double ref = std::cos(100*M_PI/180.0); // spec requires +-100 view angle range

// NEW
  // NOTE: in original-game oCNpc::CanSee (0x741c10) the horizontal sight cone is
  // abs(azimuth) < 91 deg (constant 0x5b in GetAngles). Because da is measured from
  // the reversed (target->observer) direction, the boundary must be at 180-91 = 89 deg.
  static const double ref = std::cos((180.0-91.0)*M_PI/180.0); // == cos(89 deg)
```
This widens OG's cone from +-80 to +-91 deg to match the original. The same `ref`
default is reused by `canSeeItem` (npc.cpp:4731); apply the identical change there if
matching item-sight FOV is also desired (the original item path is separate, so verify
before changing).
