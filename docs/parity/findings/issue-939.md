# Issue #939 — Jumping uphill clips the floor

**Disposition:** DEFER (real parity bug; fix needs runtime testing of the jump arc)

## OG files
- `game/game/movealgo.cpp` — `MoveAlgo::tickJumpup` (movealgo.cpp:83-112),
  `MoveAlgo::implTick` jump-arc handling (movealgo.cpp:186-225, 344-385),
  `rayMain` / `dropRay` ground sampling (movealgo.cpp:1041-1059)
- `game/world/objects/npc.cpp` — `Npc::tryJump()` (npc.cpp:4412), `tryTranslate`/`tryMove`

## Original-game behavior (prose)
In Gothic2.exe the player jump is rigid-body driven (`zCAIPlayer::StartPhysicsWithVel`,
landing in `oCAniCtrl_Human::CheckJump` 0x006b4fe0 / `CheckFallStates` 0x006b5810). The
rigid body is swept against the static collision world each step, so an ascending jump up a
slope contacts the rising ground surface continuously and the body is stopped at the floor —
the character "lands earlier" and never penetrates, exactly as the reporter notes.
// NOTE: in original-game the jump body is continuously swept against the ground, so contact
// with a rising slope halts the body before it can pass through the floor.

## OG current behavior / divergence
OpenGothic integrates the jump arc by translating with `fallSpeed` and only resolving
collision via discrete `tryMove`/`tryTranslate` calls; the upward portion of a jump
(`grav` state `JumpUp`/`InAir`) does not consult the ground ray (`dropRay`) while rising:

- `tickJumpup` (movealgo.cpp:90-97) advances `pos.y += fallSpeed.y*dt` while
  `pos.y < climbHeight`, clamping only to the *target* climb height, not to the ground
  beneath; `dropRay`/`rayMain` (movealgo.cpp:1041-1059) is read for landing but the
  ascending step can step past a slope crest.
- In `implTick`, the "above ground/void" branch is only taken when `pos.y > ground`
  (movealgo.cpp:344); when jumping *uphill* the ground rises to meet `pos.y`, and the
  fall/landing reattach (movealgo.cpp:410-415, `setState(Run)`) can occur a frame late,
  after the body has already been translated into/under the new (higher) floor.

Result: ascending into a slope, the body briefly clips below the floor surface before the
ground ray catches up and re-snaps it, instead of landing at first contact like vanilla.

## Why DEFER (implementation guide)
The fix is to clamp the ascending jump position to the swept ground each step:
- In `tickJumpup` / the `grav` ascent path, sample `dropRay` (or a forward+up sweep) and
  clamp `pos.y` to `max(pos.y, ground)` when the ground under the new XZ is above the body,
  transitioning to `Run` (land) at that contact instead of continuing the arc.
- Equivalently, perform the jump translation as a swept test (small sub-steps) so a rising
  slope stops the body at first contact.

Either approach changes the core jump-integration and landing transition for every NPC and
both game versions; it can regress normal jumps, slides, and ledge climbs, so it must be
validated interactively against the #939 repro and general traversal. Not surgical → DEFER.
