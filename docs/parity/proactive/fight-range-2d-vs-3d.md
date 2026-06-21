# Fight range uses 3D (with height) distance instead of original 2D horizontal distance

**Confidence:** High

## Original function + address
`oCNpc::IsInFightRange` (0x0067cb60) and `oCNpc::IsInDoubleFightRange` (0x0067c9a0),
both in `oNpc_Fight.cpp`.

The original computes the combat distance between the two NPCs using only the
*horizontal* plane: it takes the X and Z components of the center-to-center vector
and returns `sqrt(dx*dx + dz*dz)`. The Y (vertical) component is explicitly dropped
from the distance. It then subtracts the target's bounding-box extent along the line
(trace-ray) and compares this horizontal distance against `weaponRange + FightRangeBase`.

The vertical relationship is handled by a *separate* test, `oCNpc::IsSameHeight`
(0x00737be0), which checks whether the two bounding boxes overlap in Y with a loose
0.25 tolerance. `IsInFightRange` returns "in range" only if (horizontal distance <=
weaponRange + base) AND `IsSameHeight` is true. `IsInDoubleFightRange` performs the
same horizontal-only distance test against `base + 2*weaponRange`.

So vanilla: "in range" == horizontally close enough AND bounding boxes vertically
overlap (loosely). Height difference itself never inflates the measured distance.

## OpenGothic location
`game/game/fightalgo.cpp`:
- `qDistTo` (line 235-237) -> `Npc::fightDistanceTo(tg).quadLength()`
- `quadLength()` for `Vec3` is `x*x + y*y + z*z` (lib/Tempest/.../utility.h:135), i.e. full 3D.
- All range predicates (`isInWRange` 291, `isInGRange` 301, `isInAttackRange` 272,
  `isInCloseupRange` 284, `isInFinishRange` 278) compare this 3D squared distance
  against the squared weapon/G range.

## Divergence
OpenGothic folds the vertical height difference into the Euclidean distance, then
compares it against the weapon range. The original ignores height in the distance and
uses a separate loose bounding-box overlap test for verticality.

Effect: when a target is above or below the attacker (slopes, stairs, ledges, raised
platforms, flying creatures whose bbox still overlaps), OpenGothic reports a *larger*
distance and can flag the target out of weapon range. Vanilla NPCs would still attack
because the horizontal distance is within range and the bboxes overlap. This makes
melee AI hesitate/refuse to strike on uneven terrain where the original would hit.

## Notes / fix shape
A surgical fix would compute the distance in `fightDistanceTo` / `qDistTo` (fightalgo)
from X and Z only and add an `IsSameHeight`-style vertical-overlap gate to the range
predicates. This touches the shared distance helper and the range predicates, so it is
not a one-liner; flagged here for a targeted change rather than an exact patch.

// NOTE: in original-game IsInFightRange measures horizontal (XZ) distance and gates
// verticality separately via IsSameHeight (bbox overlap, 0.25 tol); it never adds the
// height delta into the distance compared against weapon range.
