# Low-HP `_WOUNDED` locomotion overlay never applied (DEFERRED)

**Status:** DEFERRED (visual/animation; narrow trigger band; needs runtime check
and the exact overlay-name construction).

**Confidence (divergence exists):** High. **Confidence (surgical fix):** Low.

## Original function + address

`oCNpc::CheckModelOverlays` (Gothic2.exe `0x007301d0`) is called at the end of
*every* successful `oCNpc::ChangeAttribute` (`0x0072ff60`) — i.e. on any HP / MANA
/ MAX change. It manages a single model animation overlay built from the
`_wounded` suffix (an MDS overlay such as `HUMANS_WOUNDED.MDS`, applied via the
NPC's `GetOverlay`/apply path and `oCAniCtrl_Human::InitAnimations`).

Behavior, in prose:
- If current HP (`oCNpc+0x1b8`) `< 1` it returns immediately (dead/unconscious —
  no wounded overlay bookkeeping).
- Otherwise it **removes** the `_wounded` overlay (clearing state bit `0x100` at
  `oCNpc+0x75c`) when any of: a model/type virtual check (`vtbl+0x100`) returns 0
  (non-humanoid), **HP > 2**, or `oCAniCtrl_Human::GetWaterLevel() > 1` (swimming).
- Otherwise (humanoid **and HP ≤ 2** and not swimming) it **adds** the `_wounded`
  overlay and sets bit `0x100`.

Net: a humanoid standing on land at 1–2 HP plays the wounded-locomotion overlay
(limping idle/walk), and recovers the normal overlay as soon as HP climbs above 2
(or it enters deep water).

## OpenGothic state

`Npc::changeAttribute` (`game/world/objects/npc.cpp:1244`) performs the same
clamp logic as the original (verified faithful: HP/MANA floored at 0 and capped to
their MAX, current-vs-MAX re-clamp only when the changed attribute is HP or MANA),
but it does **not** invoke any `CheckModelOverlays` equivalent. OpenGothic's only
`WOUNDED` handling is the explicit `S_WOUNDED` / `T_STAND_2_WOUNDED` **bodystate**
animations in `animationsolver.cpp:371-392` (the defeated-kneel transition), which
is a different mechanism from the HP-driven locomotion overlay. A grep for
`wounded` shows no HP-threshold overlay apply/remove.

## Divergence

A human NPC/player reduced to 1–2 HP does not switch to the wounded-locomotion
overlay in OpenGothic, and there is no overlay to remove on heal. Purely an
animation/visual difference, only in the very narrow HP∈{1,2} band.

## Why deferred

- Visual-only, and the trigger band (HP ≤ 2 absolute) is so narrow it is rarely
  observable — low gameplay value vs. risk.
- A faithful port needs the exact `_WOUNDED` MDS overlay-name construction (base
  model name + suffix) and must route through OpenGothic's overlay API
  (`AnimationSolver`/`MdlVisual` add/removeOverlay) on every `changeAttribute`,
  plus the swimming-state and humanoid gates — not a one-line surgical change.
- Needs an on-screen check to confirm the overlay resolves and looks right.

`// NOTE: in original-game oCNpc::CheckModelOverlays @0x007301d0 (called from
// oCNpc::ChangeAttribute @0x0072ff60) toggles the _WOUNDED locomotion overlay at HP<=2.`
