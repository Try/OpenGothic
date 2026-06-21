# Issue #637 — Move trigger Problems (Sleeper Temple movers)

**Disposition:** DEFER (remaining item is a runtime physics/platform-rider bug; most
sub-items already fixed by merged PRs)

## Status of the checklist (from the issue)
- 13.1 — fixed by PR #636
- 13.2 — fixed by PR #767 (death-trap roof timing)
- 13.3 — **OPEN**: Sleeper Temple elevator floor clipping; player falls through the
  rising block to the ground.
- 13.4 — fixed by PR #767 (Uriziel trap withdraw)
- 13.5 — fixed by PR #767 (arrow-target trap relax / clipping)
- 13.6 — fixed by PR #777 (2nd-room arrow puzzle lowering wall)
- "goto waypoint TPL_062" freeze (infinite trigger events) — fixed by PR #767

Only 13.3 remains.

## OG files (relevant to 13.3)
- `game/world/triggers/movetrigger.cpp` — `MoveTrigger::tick`, `advanceAnim`,
  `setLocalTransform`, `moveEvent` (drives the platform transform + physics mesh)
- `game/game/movealgo.cpp` — NPC/player ground & collision integration (platform riding)
- `game/world/objects/npc.cpp` — position/ground handling
- `game/world/triggers/abstracttrigger.cpp` — base move/physic plumbing

## Original-game behavior (prose)
In Gothic2.exe a rising `zCMover` carries entities standing on it: the engine's mover tick
(`zCMover::OnTick`, see also `oCMover`/`zCMover` movement around the keyframe interpolation)
updates the platform collision hull each step, and the physics/AI ground query in
`oCAniCtrl_Human` movement re-grounds the player onto the moved platform top surface within
the same frame, so the player rides up with the floor instead of being left behind and
clipping into the block. The key parity property is per-tick ordering: platform transform +
collision-mesh update must be applied before (or consistently with) the player's ground
resolution, and the platform top must present a solid floor at the new height each tick.

## OG current behavior / divergence
`MoveTrigger::tick` advances the animation and updates the visual + physics mesh via
`advanceAnim` → `setLocalTransform` → `moveEvent` (`movetrigger.cpp:143-147, 277-314`).
The physics mesh is rebuilt only when `cd_dynamic || cd_static` (`movetrigger.cpp:26-29`).
The player is NOT explicitly carried by the platform; ground/collision in `movealgo.cpp`
must catch the rising surface each frame. If the platform's collision top moves past the
player's feet within one tick (fast vertical segment, or update ordering relative to
`movealgo` ground query), the player ends up inside/below the block — the reported clipping.

This is a runtime collision/timing interaction, not a single confirmable surgical line.
It needs the attached savegame + video to reproduce and step.

## Why DEFER (not FIX)
- Requires runtime reproduction with the provided savegame to confirm whether the fault is
  (a) tick ordering (mover moved after player ground query), (b) missing platform-rider
  carry of the player, or (c) collision-mesh top not solid during fast vertical motion.
- A blind edit to `movealgo`/mover tick ordering risks regressing all other movers
  (boats, elevators, doors) and the already-merged #767/#777 fixes.

## Investigation guide (for the runtime fixer)
1. Reproduce with the issue savegame on the Sleeper Temple elevator; observe player Y vs
   platform top Y per tick.
2. Check update ordering in the world tick: does `MoveTrigger::tick` (and its
   `moveEvent`/physics-mesh update) run before or after `MoveAlgo` ground resolution for the
   player in the same frame? Compare against original per-tick ordering.
3. Verify the elevator mover actually has a collision mesh (`cd_dynamic`/`cd_static`);
   if not, the rising floor has no solid hull → player falls through (likely root cause).
4. If carry-the-rider is needed, add platform-velocity to the standing NPC's position in
   `movealgo.cpp` for the frame, matching original mover-rider behavior.
5. Confirm fix does not regress death-trap roof (13.2) or arrow-puzzle wall (13.5/13.6)
   timing already landed in #767/#777.
