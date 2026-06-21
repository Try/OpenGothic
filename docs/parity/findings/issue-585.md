# Issue #585 — NPC walk-teleport misalignment

Upstream: https://github.com/Try/OpenGothic/issues/585
Type: explicit deviation-from-original (vanilla) bug. **Status: arrival-snap applied (see below).**

## Issue
When an NPC walks to a waypoint / freepoint and stops, vanilla Gothic II snaps the NPC
onto the exact target spot. In OpenGothic the NPC stops wherever its arrival-radius check
first trips, leaving it visibly offset from the WP/FP ("teleport misalignment": the small
final correction the original performs is missing, so subsequent scripted actions that assume
the NPC stands *on* the spot look shifted).

## Subsystem & OG files
- `game/world/objects/npc.cpp` — `Npc::implGoTo`, `Npc::GoTo::isClose`, arrival handling, `AI_AlignToFp`.
- `game/game/movealgo.cpp` — `MoveAlgo::isClose`, `closeToPointThreshold`, root-motion/`setPosition`.
- `game/world/objects/npc.cpp:4549` — `attachToPoint` (sets currentFp only, not position).

## Original behavior (Ghidra — Gothic2.exe)
The original drives goto via an "RBT" (rigid-body move-to-target) state machine in
`oNpc_Move.cpp`. Key functions:

- `oCNpc::RbtUpdate` @ **0x686690** — per-tick. Stores the desired target into fields
  `0x4c8..0x4d0`, computes an approximate "movement extent" estimate (the cheap
  max+mid+min Manhattan-ish norm of the per-frame delta, scaled by 20.0 when grounded /
  10.0 otherwise) into `0x4ec`, computes the **squared** distance from the model translation
  to the target into `0x4e0`, and sets the "arrived" bit (flag&8) iff `dist² < threshold²`,
  where `threshold²` is held in `0x4e4`.
- `oCNpc::RobustTrace` @ **0x686ab7** — tracks the **closest approach**: keeps the smallest
  squared distance seen in `0x4dc`, and only when the NPC is at/under that closest distance
  does it invoke `RbtMoveToExactPosition`.
- `oCNpc::RbtMoveToExactPosition` @ **0x686880** — the actual snap. Guard: only proceeds when
  `0x4dc <= 0x4e0` (i.e. the NPC has reached its closest approach to the target). It then
  takes the stored target `0x4c8..0x4d0`, calls `oCVob::SearchNpcPosition` @ **0x828c…**
  (a.k.a. `SearchNpcPosition`) to validate/adjust the spot against geometry/other NPCs, and
  on success does `zCVob::SetPositionWorld(adjustedTarget)` — teleporting the NPC exactly onto
  the resolved spot — then restarts the idle/stand ani. On failure it just clears the snap bit.
- `oCNpc::RbtGotoFollowPosition` @ **0x685c…** / `EV_GotoVob` @ **0x685580**,
  `EV_GotoFP` @ **0x6858…** seed `0x4e4` (threshold²) per goto kind:
  GotoVob ≈ 40000 (200²), GotoFP ≈ 2500 (50²), follow-path nodes ≈ 6400 (80²).
- Freepoint alignment: `oCNpc::EV_AlignToFP` @ **0x683230** — after arrival, snaps the NPC's
  XYZ to the spot position plus the spot-vob offset (offsets scaled by 200.0 / 100.0 = the
  ±100 search box used by `CollectVobsInBBox3D`) and aligns heading to the spot's matrix.

**Algorithm in prose:** walk toward target each tick; remember the closest squared distance
reached; once within the goto-specific arrival radius AND at the closest-approach point, run a
geometry-validating position search and **hard `SetPositionWorld` onto the exact (validated)
target**, then drop into stand ani. For freepoints, additionally re-snap position+heading to
the spot vob (`EV_AlignToFP`). Net effect: the final NPC pose is exactly the spot, not the
"first frame that entered the radius."

## OpenGothic current behavior
- `npc.cpp:1499` `Npc::implGoTo(dt, destDist)`: moves toward target; on
  `go2.isClose(*this,destDist)` (npc.cpp:1514) it either advances the waypath
  (`go2.wp = … wayPath.pop()`, `attachToPoint`) or calls `clearGoTo()` (npc.cpp:1530).
  **There is no `setPosition` onto the exact WP/FP** — the NPC simply stops where the radius
  test first passed.
- `npc.cpp:1490-1492` arrival radius = `closeToPointThreshold*0.5` (=20) or `*1.5` (=60).
- `movealgo.cpp:10` `closeToPointThreshold = 40`; `MoveAlgo::isClose` (movealgo.cpp:700)
  is a pure `quadLength < dist*dist` radius test — no closest-approach tracking, no snap.
- `AI_AlignToFp` (npc.cpp:2803) only **turns** to the FP direction; it never re-snaps the FP
  position, unlike `EV_AlignToFP`.
- `attachToPoint` (npc.cpp:4549) records `currentFp`/`hnpc->wp` but does not move the NPC.

## Divergence hypothesis
OpenGothic treats "within arrival radius" as "done" and stops in place. The original treats
the radius only as a trigger and then performs a final exact reposition
(`RbtMoveToExactPosition` → `SearchNpcPosition` → `SetPositionWorld`, and for FPs
`EV_AlignToFP`). The missing final snap leaves the NPC offset from the spot by up to the
arrival radius (20–60 units, more for FPs), which reads as the reported misalignment.
There is also no closest-approach tracking, so OG can stop on the far edge of the radius rather
than at the nearest point the original would have snapped from.

## Proposed fix (behavioral)
In `Npc::implGoTo`, when a goto target is *reached and finalized* (the `finished` /
`clearGoTo()` path at npc.cpp:1527-1531, and the per-WP advance at 1516-1525 for the final
WP), perform a horizontal-only exact reposition onto the validated target position before
stopping the walk ani — i.e. set the NPC's X/Z to `go2.target()` (keeping engine ground/floor
alignment for Y via the existing `MoveAlgo` ground snap), guarded by a geometry/collision test
analogous to `SearchNpcPosition` (reuse `testMove`). For `AI_AlignToFp`/`AI_AlignToWp`
(npc.cpp:2803), additionally re-snap position to `currentFp->position` (+ vob offset) in
addition to the current heading turn, mirroring `EV_AlignToFP`.

```cpp
// NOTE: in original-game oCNpc::RbtMoveToExactPosition @0x686880 (oNpc_Move.cpp), on arrival
// the NPC is hard-SetPositionWorld'd onto the SearchNpcPosition-validated target (not left at
// the radius edge); RobustTrace @0x686ab7 only snaps at closest approach (0x4dc<=0x4e0), and
// EV_AlignToFP @0x683230 additionally re-snaps FP position+heading. closeToPointThreshold here
// is only a trigger, not the final pose.
```

Keep Y resolution via the existing gravity/ground path; only correct X/Z to avoid floating/clipping.

## Status
**Partially applied.** The arrival exact-reposition is implemented in
`game/world/objects/npc.cpp` `Npc::implGoTo`, in the `if(finished)` block before
`clearGoTo()`: when not chasing an NPC (`go2.npc==nullptr`) it snaps X/Z onto the
destination captured in the local `target` (the value read *before* the waypath pop, so it
is the real destination, not the post-pop stale `go2.target()`), keeps Y for the ground
path, and reverts via `setPosition(prev)` if `hasCollision()` — mirroring the original's
`SearchNpcPosition` geometry guard and the existing `Interactive::setPos` pattern. Builds
clean.

Deferred (lower priority, more speculative): the `EV_AlignToFP` position re-snap in the
`AI_AlignToFp`/`AI_AlignToWp` handler (npc.cpp:2802) — it still only turns to the FP
direction. The arrival snap above already removes the up-to-radius offset that caused the
reported misalignment. Behavioral verification (walk an NPC to a WP/FP and check final
position) needs a playtest.
