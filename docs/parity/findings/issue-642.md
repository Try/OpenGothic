# Issue #642 — Cannot jump-climb to a ledge from standing when forward space is free

**Disposition:** DEFER (real parity bug; needs runtime testing across ledge geometry)

Closely related: #909 (same root cause), #312 (general edge-grab divergence).

## OG files
- `game/world/objects/npc.cpp` — `Npc::tryJump()` (npc.cpp:4412-4508)
- `game/game/playercontrol.cpp` — Jump-key dispatch (playercontrol.cpp:879-911)
- `game/game/movealgo.cpp` — `MoveAlgo::startClimb`, `tickJumpup`, `tickClimb`

## Original-game behavior (prose)
In Gothic2.exe the player's jump key drives `oCAniCtrl_Human` which evaluates a *ledge
climb* independently of whether horizontal forward space is free:
- `oCAniCtrl_Human::CanJumpLedge` (0x006b2050) calls
  `zCAIPlayer::DetectClimbUpLedge` (0x0050fd90) + `zCAIPlayer::GetFoundLedge`
  (0x0050fd00). `DetectClimbUpLedge` ray-casts upward/forward to probe for a grabbable
  ledge edge and computes its height; if a ledge is found it returns a climb class
  (1/2/3 = low/mid/high) by comparing ledge height against the guild jump thresholds.
- `WallInFront` (0x006aebf0) and a forward-jump path (`PC_JumpForward` 0x006b1e00) exist
  separately. The ledge probe does NOT require a wall directly in front nor that forward
  space be blocked — a ledge above with open space below still produces a climb.
// NOTE: in original-game ledge detection (DetectClimbUpLedge) is performed regardless of
// free forward space; a forward jump is only chosen when no climbable ledge is found.

## OG current behavior / divergence
`Npc::tryJump()` short-circuits to a plain forward jump as soon as horizontal space is
clear:

```
4435  if(!mvAlgo.isJumpUp() && physic.testMove(pos0+dp,info)) {
4436    // jump forward
4437    ret.anim   = Anim::Jump;
4438    ret.noClimb = true;
4439    return ret;
4440    }
```

`testMove(pos0+dp,info)` returns true when the body can translate forward by `climbMove`
(55cm) without collision. The ledge ray (`landRay` at npc.cpp:4442) and the climb classes
(`JumpUpLow/Mid`, `JumpHang`) are only reached when forward space is blocked. So when the
player stands slightly back from a wall whose top is a grabbable ledge (the #642 / #909
repro), OpenGothic always picks `Anim::Jump` (a forward hop) and never the climb.

The caller confirms this: `playercontrol.cpp:901-906` only climbs when
`jump.anim != Npc::Anim::Jump`.

## Why DEFER (implementation guide)
The correct fix is to evaluate the ledge probe *before* (or independently of) the
forward-space short-circuit, matching the original ordering:
1. Cast the upward/forward ledge ray first (the `landRay` at +`jumpUp+jumpLow` already in
   `tryJump`).
2. If a climbable ledge is found within `jumpUp` and the approximate climb path is clear
   (`testMove(pos1,pos0)` / `testMove(pos2,pos1)`), return the climb anim — even if
   `testMove(pos0+dp)` would have succeeded.
3. Only fall back to `Anim::Jump` when no ledge is detected.

This reorders behavior that currently gates many cases (ordinary forward jumps over gaps,
running jumps, jumps toward sloped-but-not-climbable surfaces). It risks regressing
forward-jump feel and false-positive climbs onto non-ledge geometry, so it must be
validated interactively against the issue save plus general traversal (Khorinis rooftops,
gaps, slopes). Not a one-line surgical change → DEFER.
