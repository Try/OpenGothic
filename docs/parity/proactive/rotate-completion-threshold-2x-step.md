# NPC turn completion snaps from 2x the per-frame step instead of clamping to one step

**Confidence:** Medium-High

## Original function + address (prose only)

The per-frame heading update for an NPC goto/turn is `oCNpc::Turning` @ `0x00683120`.
Its logic, in prose:

1. It calls `oCNpc::GetAngles` @ `0x006812b0` to obtain the horizontal angle delta from
   the NPC's current facing (At-vector) to the target. `GetAngles` finishes by converting
   the azimuth to degrees and normalizing it into `[-180,180]` with a single `±360`
   correction, so the returned `angleToTarget` is the true signed shortest-arc delta.
2. It computes a per-frame step `= GetTurnSpeed()` (`@0x00680970`, the guild/NPC turn
   speed in deg/s) `* frameDeltaMs` (the global frame-time `DAT_0099b3ec`).
3. It **clamps the rotation applied this frame to `[-step, step]`**: if
   `angleToTarget >= 0` it rotates by `min(angleToTarget, step)`, else by
   `max(angleToTarget, -step)`. It then calls `oCAniCtrl_Human::TurnDegrees`
   (`@0x006aeb10`), which does `zCVob::RotateWorld({0,1,0}, clampedDelta)`.
   Because the applied rotation is the *clamped* delta, when the remaining angle is
   within one `step` the NPC rotates by exactly the remaining amount and **lands exactly
   on the target** — the maximum heading change in any single frame is `step`.
4. Separately, it stops the turn animation (`StopTurnAnis`) only when
   `abs((int)angleToTarget) < 5` degrees (a fixed 5-degree threshold, unrelated to step).

The unit equivalence between OG's `step` and the original `GetTurnSpeed()*frameDeltaMs`
is already established in `docs/parity/proactive/turn-faiturn-2x-combat-rate.md`
(OG `turn_speed` is deg/s; `rotateTo` does `step *= dt/1000`).

## OpenGothic file:line

`game/world/objects/npc.cpp:3694` — `Npc::rotateTo`, specifically the completion/snap
gate at lines 3706-3714:

```
float a  = angleDir(dx,dz);
float da = a-angle;

if(anim == AnimationSolver::TurnType::None || std::cos(double(da)*M_PI/180.0)>0) {
  if(float(std::abs(int(da)%360))<=(step*2.f)) {   // line 3710
    setAnimRotate(0);
    setDirection(a);                                // exact snap
    return false;
    }
  }
```

and the non-snap branch at line 3735, which always rotates by the full `step`:

```
setDirection(angle - float(rot)*step);
```

## Divergence

OpenGothic does **not** clamp the per-frame rotation to the remaining angle. When the
NPC is *not* within the completion window it always rotates by a full `step`
(line 3735), and it only "lands" on the target by *snapping* (`setDirection(a)`) once
the remaining angle is within **`step*2`** (line 3710). The original instead clamps the
applied rotation to `±step` every frame, so its maximum per-frame heading change is
exactly `step` and it lands on the target by rotating the (smaller) remaining amount.

Consequence: during the final approach OpenGothic can change the heading by up to
**2x `step`** in a single frame (e.g. remaining angle `1.8*step` -> snapped to target in
one frame), whereas the original never rotates more than `step` per frame and would take
two frames (`step`, then the remaining `0.8*step`). This is a frame-rate- and
turn-speed-dependent over-snap: at low FPS / high `turn_speed` the over-rotation window
(`step*2`) can be tens of degrees, making OG complete turns one frame early with a
visibly larger instantaneous heading jump than `Gothic2.exe`. Distinct from the deferred
faiTurn-2x combat multiplier (which scales `step` itself) and the steer-avoidance
hardcoded rate.

## Proposed patch

Change the completion window from two steps to one step, so the snap reproduces the
original's clamp-to-one-step exact landing (when the remaining angle is `<= step` the
clamp would rotate by exactly the remaining amount, which `setDirection(a)` matches
end-state-for-end-state; when `> step` the original rotates a full `step`, identical to
line 3735).

OLD (`game/world/objects/npc.cpp:3709-3714`):
```
  if(anim == AnimationSolver::TurnType::None || std::cos(double(da)*M_PI/180.0)>0) {
    if(float(std::abs(int(da)%360))<=(step*2.f)) {
      setAnimRotate(0);
      setDirection(a);
      return false;
      }
    }
```

NEW:
```
  if(anim == AnimationSolver::TurnType::None || std::cos(double(da)*M_PI/180.0)>0) {
    // NOTE: in original-game oCNpc::Turning @0x00683120 the per-frame rotation is
    // clamped to [-step,step] (oCAniCtrl_Human::TurnDegrees @0x006aeb10), so the max
    // heading change per frame is one step and the NPC lands exactly on target within
    // a single step; the completion window is therefore step, not step*2.
    if(float(std::abs(int(da)%360))<=step) {
      setAnimRotate(0);
      setDirection(a);
      return false;
      }
    }
```

Grep-verified OG symbols used: `Npc::rotateTo`, `angleDir`, `setDirection`,
`setAnimRotate`, `step` (all in `game/world/objects/npc.cpp`).

Note on the secondary divergence (turn-animation stop at a fixed 5 degrees vs OG's
`setAnimRotate(0)` firing at the step-based window): this only governs which turn
animation plays, not the heading, and the fixed-5-deg vs step-based choice is framerate
dependent feel-tuning, so it is left **DEFERRED**; only the heading-jump threshold above
is proposed.
