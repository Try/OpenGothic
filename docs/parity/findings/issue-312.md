# Issue #312 — Climbing / edge-grabbing detection differs from the original

**Disposition:** DEFER (systemic ledge-detection divergence; needs runtime testing)

Related: #642 and #909 are concrete instances of the same `tryJump` divergence.

## OG files
- `game/world/objects/npc.cpp` — `Npc::tryJump()` (npc.cpp:4412-4508)
- `game/game/movealgo.cpp` — `MoveAlgo::startClimb` (movealgo.cpp:714),
  `tickJumpup` (movealgo.cpp:83), `tickClimb` (movealgo.cpp:114), `climbMove=55`
- `game/game/playercontrol.cpp` — Jump-key dispatch (playercontrol.cpp:879-911)

## Original-game behavior (prose)
Edge grabbing in Gothic2.exe is a dedicated ledge-probe, not a side effect of a blocked
forward move:
- `oCAniCtrl_Human::CanJumpLedge` (0x006b2050) → `zCAIPlayer::DetectClimbUpLedge`
  (0x0050fd90), which casts rays from the body up and forward to locate a grabbable edge
  and its world-space height/normal, then `GetFoundLedge` (0x0050fd00) reports success.
- The detected ledge height is compared to the guild jump thresholds to pick a climb class
  (returns 1/2/3). The probe runs from a standing position and from `JUMPUP` mid-air state,
  so the player can grab an edge while rising as well as from the ground.
- The ledge search window and forward reach are tuned to the original's distances, which is
  why standing on a street-light and grabbing Thorben's roof (the #312 repro) works
  reliably in vanilla.
// NOTE: in original-game ledge grabbing is driven by an explicit ray probe
// (DetectClimbUpLedge) with the original's reach/height window, independent of whether the
// forward step is blocked.

## OG current behavior / divergence
OpenGothic folds climb detection into `Npc::tryJump()`, which:
1. Returns a plain forward `Anim::Jump` the moment `physic.testMove(pos0+dp)` succeeds
   (npc.cpp:4435) — so an open gap below a reachable ledge defeats the climb (see #642/#909).
2. Uses a single forward reach of `climbMove = 55` cm (movealgo.cpp:11) plus a
   `landRay` at `pos0+dp + (jumpUp+jumpLow)` (npc.cpp:4442) for the ledge sample. This
   geometry and reach do not match the original's `DetectClimbUpLedge` window, so grabs
   that succeed in vanilla (rooftop/wall transitions in Khorinis) miss the ledge sample and
   fail.

The net effect is the "works differently between original and opensource" behavior the
reporter describes: edge grabs that are reliable in vanilla are inconsistent here.

## Why DEFER (implementation guide)
A faithful fix means porting the original ledge-probe shape:
- Decouple ledge detection from the forward-space test (as in #642).
- Cast the ledge probe with the original's forward reach and vertical window rather than the
  single `climbMove`/`landRay` sample, including the rising-`JumpUp` case so an edge can be
  grabbed mid-jump.
- Re-tune `climbMove` / sample offsets against measured original distances.

These are interlocking geometry changes that alter traversal feel game-wide and must be
validated interactively (the issue cites specific Khorinis locations). No surgical patch is
safe without that testing → DEFER. Fixing #642's ordering is the recommended first step.
