# AI dodge / strafe direction-selection is not gated on free space

**Confidence:** Medium-High that the divergence is real; **fix is DEFERRED** (collision-blocked).

## Original function + address (prose only)

- `oCNpc::ThinkNextFightAction` (Gothic2.exe @0x0067e350) is the per-tick executor for the
  fight-AI move chosen by `FindNextFightAction`. For the **STRAFE** action it calls
  `oCNpc::CanStrafe(left?, strafeAniId)` (@0x00688670) once for the left strafe ani
  (anictrl+0x1030) and once for the right strafe ani (anictrl+0x1034). It then strafes only
  toward a side that reported free space: if both sides are free it coin-flips (`rand()&1`);
  if exactly one side is free it always takes that side; if **neither** side is free it does
  nothing (falls through to `_Stand`). For the **JUMP_BACK** action (FAI move code 3) it
  builds `T_<weapon>JUMPB`, calls `oCNpc::CanJumpBack(aniId)` (@0x006888d0), and **aborts the
  jump-back entirely (returns, plays nothing) when there is no free space behind the NPC.**
- `oCNpc::CanStrafe` / `oCNpc::CanJumpBack` both compute a probe distance from the strafe/hop
  animation's root translation length (`zCModelAni::GetAniTranslation`, +20.0; default 150.0
  units) plus a vertical margin (npc water-depth or +100.0), then run the world collision
  trace `FUN_006b6a90` against that swept box. They return non-zero only when the swept volume
  is clear.
- `oCNpc::EV_Dodge` (Gothic2.exe @0x00685290), the message-driven dodge (script `AI_Dodge`),
  follows the same priority: `CheckEnoughSpaceMoveBackward` → play `T_JUMPB`; else
  `CheckEnoughSpaceMoveLeft` → play left strafe ani; else `CheckEnoughSpaceMoveRight` → play
  right strafe ani; else do nothing.

## OpenGothic file:line

- `game/game/fightalgo.cpp:136-140` — STRAFE expands to a blind `rand(2)` choice of
  `MV_STRAFEL`/`MV_STRAFER` with no space test.
- `game/world/objects/npc.cpp:1771-1805` — `MV_STRAFEL`/`MV_STRAFER` play `Anim::MoveL`/
  `Anim::MoveR` unconditionally (only soft-lock guard `hasAnim`).
- `game/world/objects/npc.cpp:1807-1827` — `MV_JUMPBACK` plays `Anim::MoveBack`
  unconditionally except for an OpenGothic-specific `isInFocusAngle` (30°) facing gate.
- `game/world/objects/npc.cpp:2827-2834` — `AI_Dodge` queue action plays `Anim::MoveBack`
  with no left/right fallback.
- Grep confirms OpenGothic has **no** `CanStrafe` / `CanJumpBack` / `CheckEnoughSpaceMove*`
  equivalent anywhere under `game/`.

## Divergence

In the original, the strafe **direction** and the jump-back **go/no-go** are decided by a
collision sweep of the move's animation translation. OpenGothic instead:

1. picks the strafe side by a pure coin flip, so an NPC backed against a wall on one side
   strafes *into* the wall ~50% of the time (the original would always take the open side); and
2. performs a jump-back regardless of whether there is space behind it, so an NPC with its
   back to a wall still plays `T_JUMPB` (the original suppresses it). OpenGothic partially
   compensates for (2) with an *added* 30° facing cone that the original does not have, which
   in turn suppresses some FAI-scripted jump-backs the original would allow — a second-order
   divergence in the opposite direction.

## Proposed patch

**DEFERRED.** A faithful fix requires the original's `CanStrafe` / `CanJumpBack` behaviour:
sweeping the strafe/jump-back animation's root translation (≈ani-length+20, with the +100
vertical margin) through the world collision and accepting the move only when the box is
clear. That depends on `oCNpc::CheckEnoughSpaceMove{Backward,Left,Right}` /
`zCAIPlayer`-style swept-box collision (`FUN_006b6a90`), which OpenGothic does not implement
for fight movement. Approximating it with a single ray (as done elsewhere in the MV_ATTACK
path via `owner.physic()->ray`) would change feel without matching the swept-box semantics and
risks new false negatives. Per the clean-room rules ("empty beats false positives";
feel-tuning and collision-shaped reimplementations are out of scope), no surgical
build-verifiable patch is proposed.

// NOTE: in original-game oCNpc::ThinkNextFightAction @0x0067e350 the STRAFE move calls
// oCNpc::CanStrafe @0x00688670 for each side and strafes only toward free space; JUMP_BACK
// calls oCNpc::CanJumpBack @0x006888d0 and is skipped entirely when no space is behind the NPC.
