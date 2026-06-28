# swim/jump-up: ledge-grab / climb-up forward reach distance (climbMove = 55)

**Confidence:** Medium (DEFERRED — not a clean build-verifiable numeric swap)

## Original fn + address (prose)
The forward reach used by the original to decide a running-jump vs. a climb-up-onto-ledge
lives in two cooperating routines. `oCAniCtrl_Human::JumpForward` @0x006b21e0 builds the
forward obstacle-detection trace ray by scaling the model's "at" vector by the immediate
float constant `0x43160000` = **150.0** units (with an additional vertical lift driven by
`fsin(pitch) * 150`). `zCAIPlayer::DetectClimbUpLedge` @0x0050fd90 — the routine that actually
locates a grabbable ledge for `CanJumpLedge`/`JumpForward`/`Swim_CanClimbLedge` — projects its
ledge probe forward by `_DAT_00831508 + 0x43160000` (collision-front extent **+ 150.0**), and
caps the reachable climb height at `GetJumpUpHeight() * 0x3f733333` (× **0.95**). So the
original's effective forward ledge-grab reach is roughly *collision radius + 150* measured from
the body's front face.

## OG file:line
- `game/game/movealgo.cpp:11` — `const float MoveAlgo::climbMove = 55;`
- `game/world/objects/npc.cpp:4754` — `float len = MoveAlgo::climbMove;` then
  `dp = {len*c,0,len*s}` is the *only* forward probe `Npc::tryJump()` uses for both the
  "wall ahead → plain Jump" test (`physic.testMove(pos0+dp)`) and the ledge `landRay`/climb
  decision.
- `game/game/movealgo.cpp:100,120` — the same `climbMove` is reused as the horizontal
  pull-in distance during `tickClimb` (`v = {0,0,climbMove}`).

## Divergence
OpenGothic probes only **55** units forward from the NPC *center* to detect both a blocking
wall and a climbable ledge, whereas the original probes ~**150** units forward from the
collision *front face* (≈150 + radius from center). OpenGothic's reach is on the order of
3× shorter, so the player must stand noticeably closer to a ledge before the JumpUp/climb
animations trigger, and forward jumps register blocking geometry later than the original.

## Proposed patch
DEFERRED. Reasons:
1. **Frame mismatch, not a 1:1 constant.** The original 150 is measured from the collision
   object's front face plus a separate ray cast against world geometry inside
   `DetectClimbUpLedge`; OpenGothic measures 55 from the NPC origin using `physic.testMove`.
   A literal `55 → 150` substitution would over-reach (origin + 150 ≈ 185 ≈ original + ~35
   radius) only by coincidence, and is not derivable to the exact original behavior without
   replicating the front-face + cap-at-0.95×jumpHeight probe geometry.
2. **`climbMove` is overloaded.** The same constant also sets the climb pull-in translation in
   `tickClimb` (`movealgo.cpp:100,120`); changing it to match the *detection* reach would
   simultaneously alter how far the NPC slides forward while pulling up onto the ledge, a
   second behavioral change with no matching original constant.
3. Therefore not a surgical, build-verifiable single-value fix. A faithful port would replace
   the single `climbMove` probe in `tryJump` with a front-face forward ray of length 150 and a
   `0.95 * jumpUpHeight` climb-height cap, kept independent of the `tickClimb` pull-in distance
   — a larger change than this proactive pass should land blind.

// NOTE: in original-game oCAniCtrl_Human::JumpForward @0x006b21e0 / zCAIPlayer::DetectClimbUpLedge
// @0x0050fd90 the forward jump/ledge-grab probe is (collisionFront + 150.0) units with the climb
// height capped at GetJumpUpHeight()*0.95; OpenGothic's MoveAlgo::climbMove=55 single-probe from the
// NPC origin gives a markedly shorter reach.
