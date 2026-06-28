# Melee reach distance metric (horizontal-vs-3D) and same-height gate

**Confidence:** N/A — NO FINDING (verified parity)

## Original function + address

The actual melee connect in the original is driven by `oCAniCtrl_Human::CheckHitTarget`
(Gothic2.exe 0x006b05c0): it walks the weapon/limb model nodes via `CheckModelLimbCollision`
and, for the colliding vob, gates the strike on `oCNpc::IsInFightRange`
(Gothic2.exe 0x0067cb60, taking `zCVob* target, float& outDist`). Inside `IsInFightRange`
the reach metric is computed as follows:

- Take both vobs' world translations. The stored distance is
  `sqrt(dx*dx + dz*dz)` — i.e. the **horizontal XZ-plane distance with the Y (dy)
  component dropped entirely**. It is NOT a full 3D distance.
- That distance is then *reduced* by the penetration depth of a ray (cast from the
  target toward the attacker, length 4x) against the target's world bbox, so the
  effective distance is measured to the target's body surface rather than its center.
  For two specific monster guilds (47 / 45) the target bbox is first scaled by
  (0.5, 1.0, 0.5).
- The elevation gate is `oCNpc::IsSameHeight` (Gothic2.exe 0x00737be0): combatants
  count as same-height when their world bboxes overlap vertically, allowing a gap of
  up to `0.25 * targetHeight` (the tolerance is always based on the **target**, and
  the comparison branch picks attacker-below-target vs attacker-above-target on world Y).
- Final reach test: `effectiveDist <= attackerFightRangeBase + weaponRange`, where
  `weaponRange = GetFightRange`-style `*(item+0x26c)` (item range) or the fist range,
  added to the attacker's own base (`oCNpc::GetFightRange` @0x0067cd80).

## OpenGothic file:line

- `game/game/fightalgo.cpp:273` `FightAlgo::qDistTo` — returns horizontal
  `d.x*d.x + d.z*d.z` (Y dropped) and returns `1e30f` when `fightSameHeight` fails.
- `game/game/fightalgo.cpp:260` `fightSameHeight` — `tol = 0.25*(bMax-bMin)` of the
  **target** bbox; branch on `a.position().y <= b.position().y` then
  `aMax-tol >= bMin` / `bMax-tol >= aMin`.
- `game/world/objects/npc.cpp:2085` `Npc::commitDamage` (hit-frame `def_opt_frame`,
  `npc.cpp:2493`) gates the actual strike on `isInAttackRange` (-> `qDistTo`) and
  `isInFocusAngle`. Used for both player and NPC swings.
- `game/game/fightalgo.cpp:318` `isInAttackRange` compares `qDistTo` to
  `prefferedAttackDistance = baseTg + baseNpc + weaponRange` (squared).

## Divergence

None on the metric or elevation gate. OpenGothic already:
1. Drops Y and measures horizontal XZ distance (`qDistTo`), matching
   `IsInFightRange`'s `sqrt(dx^2+dz^2)`. This is the same class as the focus-range fix
   in `oCNpc::FocusCheck`; it is already applied here.
2. Reproduces `IsSameHeight` faithfully in `fightSameHeight` — same `0.25*targetHeight`
   tolerance, same below/above branch direction, same min/max comparisons.

The two places where OG differs are deliberate, vanilla-tuned approximations of the
original's bbox-penetration refinement, and they coincide with already-deferred items:
- OG adds the **target's** `fight_range_base` (`baseTg`) to the reach instead of
  subtracting the target's bbox penetration depth; the per-guild bbox scaling and the
  weapon-range/guild-bonus shape belong to the deferred `reach-melee-1ha2ha-guild-bonus`.
- OG substitutes a 30-degree `isInFocusAngle` front cone for the original's exact
  per-limb `CheckModelLimbCollision`, which belongs to the deferred
  `hitframe-window-limb-cleave` / `hitwin-preopt-collision-window`.

## Proposed patch

DEFERRED — no fresh divergence. The horizontal-vs-3D metric question is already
resolved (`qDistTo` zeroes Y) and the `IsSameHeight` elevation gate is reproduced
correctly. The remaining differences (bbox-penetration approximation, limb-collision
cone) fall under the four excluded/deferred melee items. NO FINDING.
