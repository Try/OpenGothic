# AI_AlignToWP aligns to current free-point instead of the nearest waynet waypoint

**Confidence:** High

## Original function + address

The Daedalus external `AI_AlignToWP` (Gothic2.exe `DefineExternals_Ulfi`, bound to the
external body at `0x006ee3a0`) does *not* re-use the free-point/spot alignment path. Instead it:

1. Takes `self`, reads the NPC's home world (`oCNpc+0xb8`) and that world's waynet (`world+0x90`).
2. Calls `zCWayNet::GetNearestWaypoint` (`0x007ad660`) to find the spatially **nearest waynet
   waypoint** to the NPC's position.
3. Builds an `oCMsgMovement` (`oCMsgMovement::oCMsgMovement` `0x00765a30`) with subtype **5 =
   EV_TurnToPos**, whose target position is `npcPos + waypoint.dir * 200` (the waypoint's
   stored direction vector, columns `wp+0x50/0x54/0x58`, scaled by `200.0` constant `0x00830030`).
4. Posts that turn message, so the NPC rotates to face the nearest waypoint's heading.

By contrast `AI_AlignToFP` external (`0x006ebdc0`) posts subtype **0x11 = EV_AlignToFP**
(`oCNpc::EV_AlignToFP` `0x00683230`), which searches the bbox for the `zCVobSpot` free-point the
NPC owns and aligns to *that spot's* direction. The two externals use **different heading
sources**: AlignToWP → nearest waynet waypoint; AlignToFP → owned free-point spot. (There is no
`EV_AlignToWP`; movement subtype 0x10 is unhandled — AlignToWP is entirely external-side.)

## OpenGothic file:line

`game/world/objects/npc.cpp:2965-2974` — `case AI_AlignToWp:` and `case AI_AlignToFp:` share one
body that reads `currentFp->dir` for both.

```cpp
case AI_AlignToWp:
case AI_AlignToFp:{
  if(auto fp = currentFp){
    if(fp->dir.x!=0.f || fp->dir.z!=0.f){
      if(implTurnTo(fp->dir.x,fp->dir.z,AnimationSolver::TurnType::Std,dt))
        queue.pushFront(std::move(act));
      }
    }
  break;
  }
```

## Divergence

OpenGothic aligns `AI_AlignToWp` to `currentFp` — the last point the NPC *attached* to. When that
point is a free-point (after a prior `AI_GotoFP`/`AI_AlignToFP`) or is null (NPC never attached),
the resulting heading is wrong or the alignment silently does nothing. The original always performs
a fresh **nearest-waynet-waypoint** spatial lookup and turns to *that waypoint's* direction,
independent of whatever free-point the NPC last touched. (`AI_AlignToFP` using `currentFp` is fine
as a free-point proxy and is left unchanged.)

## Proposed patch

Grep-verified OG symbols: `World::findWayPoint(const Tempest::Vec3&)` (world.h:59, returns the
nearest waynet waypoint via `WayMatrix::findWayPoint`, waymatrix.cpp:62, iterating `wayPoints`),
`Npc::position()` → `Tempest::Vec3` (npc.cpp:644), `WayPoint::dir` is `Tempest::Vec3`
(waypoint.h:32), `Npc::implTurnTo(float,float,AnimationSolver::TurnType,uint64_t)` (npc.cpp:1521),
`owner` is the owning `World&` (e.g. `owner.findNextFreePoint`, `owner.detectNpc(position(),...)`).

OLD:
```cpp
    case AI_AlignToWp:
    case AI_AlignToFp:{
      if(auto fp = currentFp){
        if(fp->dir.x!=0.f || fp->dir.z!=0.f){
          if(implTurnTo(fp->dir.x,fp->dir.z,AnimationSolver::TurnType::Std,dt))
            queue.pushFront(std::move(act));
          }
        }
      break;
      }
```

NEW:
```cpp
    case AI_AlignToWp:{
      // NOTE: in original-game AI_AlignToWP external (Gothic2.exe @0x006ee3a0) aligns to the
      // direction of the *nearest waynet waypoint* (zCWayNet::GetNearestWaypoint @0x007ad660),
      // posting EV_TurnToPos toward npcPos + waypoint.dir*200 -- not to the current free-point.
      if(auto wp = owner.findWayPoint(position())){
        if(wp->dir.x!=0.f || wp->dir.z!=0.f){
          if(implTurnTo(wp->dir.x,wp->dir.z,AnimationSolver::TurnType::Std,dt))
            queue.pushFront(std::move(act));
          }
        }
      break;
      }
    case AI_AlignToFp:{
      if(auto fp = currentFp){
        if(fp->dir.x!=0.f || fp->dir.z!=0.f){
          if(implTurnTo(fp->dir.x,fp->dir.z,AnimationSolver::TurnType::Std,dt))
            queue.pushFront(std::move(act));
          }
        }
      break;
      }
```
