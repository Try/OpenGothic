# Waypoint-following reach radius is a fixed 150 units in the original, not closeToPointThreshold (40 -> 20/60)

**Confidence:** High on the original constant and the divergence; the proposed change is **DEFERRED** (architectural, see below).

## Original function + address (prose)

The robust-trace "destination reached" radius lives in the `oCNpc` instance field at
offset **+0x4e4**, and it is interpreted as a **squared** distance: `oCNpc::RobustTrace`
@ **0x00686960** computes the current squared distance to the active target with
`zVEC3::Length2` (squared magnitude, stored at +0x4dc) and flips the "reached" bit
(+0x4c4 bit 0) when `dist² (+0x4dc) < threshold (+0x4e4)`. `oCNpc::RbtUpdate`
@ **0x00686690** does the same comparison against +0x4e4 with the squared straight-line
distance it stores at +0x4e0. So +0x4e4 holds *radius²*.

The default value of that field is seeded by `oCNpc::RbtReset` @ **0x00686560**, which
writes `+0x4e4 = 0x46afc800` = **22500.0**, i.e. a reach radius of `sqrt(22500) = 150`
units.

This default is exactly what is used for ordinary waynet path following:

- `AI_GotoWP` / goto-by-name routes through `oCNpc::EV_GoRoute` @ **0x006834d0**
  (movement sub-type 3 in the `oCNpc::OnMessage` @0x0074b020 dispatch). `EV_GoRoute`
  builds a `zCRoute` with `zCWayNet::FindRoute` and then, for each ordinary waypoint hop,
  posts an `oCMsgMovement` of **sub-type 0** (`EV_RobustTrace`) carrying that waypoint's
  world position.
- `oCNpc::EV_RobustTrace` @ **0x00683c80** (sub-type 0) calls `RbtReset` then `RbtUpdate`
  and does **not** override +0x4e4. Because `RbtReset` just re-seeded it to 22500.0, every
  waypoint hop and the final waypoint of an `AI_GotoWP` route are considered "reached"
  when the NPC is within **150 units** of the target waypoint. After the reach bit trips,
  `RbtMoveToExactPosition` @0x00686880 hard-snaps the NPC onto the validated position.

The sibling movement handlers override +0x4e4 to their own per-message radii (already
documented elsewhere): `EV_GotoVob` -> 40000.0 (=200, AI_GotoNpc),
`EV_GotoFP` -> 2500.0 (=50), and `EV_GotoPos` (sub-type 1) -> `0x461c4000` = 10000.0
(=**100**). The plain waypoint-route case keeps the `RbtReset` default of **150**.

## OpenGothic file:line

`game/world/objects/npc.cpp:1558-1576` (`Npc::implGoTo(uint64_t)`), which picks the
arrival distance for every non-enemy goto:

```
dist = MoveAlgo::closeToPointThreshold*0.5f;          // 40*0.5 = 20
if(!mvAlgo.checkLastBounce())
  dist = MoveAlgo::closeToPointThreshold*1.5f;         // 40*1.5 = 60
if(go2.wp!=nullptr && go2.wp->useCounter()>1)
  dist = float(MAX_AI_USE_DISTANCE);
```

passed to `Npc::GoTo::isClose` (`npc.cpp:52`) -> `MoveAlgo::isClose(npc, WayPoint, dist)`
(`game/game/movealgo.cpp:719`). `closeToPointThreshold` is defined as **40** at
`movealgo.cpp:10`. The same `implGoTo` block advances `wayPath` waypoint-by-waypoint
(`npc.cpp:1596-1605`), so this 20/60-unit threshold is also the **per-waypoint advance**
distance, where the original engine advances at **150**.

## Divergence

For the bread-and-butter case of an NPC walking a waynet route (`AI_GotoWP` and all
daily-routine waypoint travel), the original engine treats a waypoint as reached / advances
to the next waypoint at a fixed **150-unit** radius (`RbtReset` default +0x4e4 = 22500.0,
tested squared in `RobustTrace`). OpenGothic instead uses
`closeToPointThreshold (40)` scaled to **20 or 60 units** (140 only when `useCounter>1`).
The OG threshold is ~2.5-7.5x tighter, so OpenGothic NPCs walk noticeably further into /
closer to each waypoint before popping it and steer through corners differently than the
original; the discrete arrival constant (150) is unambiguous in the binary.

## Proposed patch

**DEFERRED.** The constant itself is solid (22500.0 -> 150-unit reach), but it cannot be
adopted by a one-line change:

1. OpenGothic deliberately uses a *small* threshold here — the in-code rationale is
   "use smaller threshold, to avoid edge-looping in script" (`npc.cpp:1569`). The original's
   150 works because `EV_GoRoute` steers the NPC *along the way-segment geometry* via
   `oCAniCtrl_Human`, using 150 only to decide *when to switch to the next waypoint*;
   OpenGothic follows waypoint-to-waypoint by walking straight at each `wp->position()`,
   so popping a waypoint 150 units early would make NPCs cut corners and skip geometry on
   tight paths.
2. This overlaps the already-deferred waynet steering items (octagonal trace / segment
   following). Changing only the reach radius without the original's segment-based steering
   would regress path fidelity rather than improve it.

Recorded so the value is available if/when the segment-based follow model is implemented.

```
// NOTE: in original-game oCNpc::RbtReset @0x00686560 the robust-trace reach radius field
// (+0x4e4) defaults to 22500.0 and is tested squared in oCNpc::RobustTrace @0x00686960
// (dist^2 < 22500 => reached), i.e. a fixed 150-unit waypoint advance/arrival radius for
// AI_GotoWP routes (EV_GoRoute @0x006834d0 -> EV_RobustTrace @0x00683c80, which does not
// override +0x4e4). EV_GotoPos overrides it to 10000.0 (=100), EV_GotoFP to 2500.0 (=50),
// EV_GotoVob to 40000.0 (=200). OpenGothic uses closeToPointThreshold(40)->20/60 instead;
// adopting 150 requires the original's segment-based steering (deferred waynet follow).
```
