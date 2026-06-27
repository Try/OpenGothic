# AI_GotoNpc stop distance uses fight-range metric instead of fixed 200 units

**Confidence:** High (on the original behavior and routing); Medium-High on gameplay magnitude.

## Original function + address (prose)

The Daedalus external `AI_GotoNpc` (string `AI_GotoNpc` @0x008b5254) is registered/dispatched
from the externals binder `FUN_006eb110` (oGameExternal.cpp). It does not move the NPC directly;
it constructs an `oCMsgMovement` with sub-type **2** targeting the destination vob and posts it to
the target NPC's event manager.

`oCNpc::OnMessage` @0x0074b020 switches on the movement message sub-type (field +0x24) and routes
sub-type **2** to `oCNpc::EV_GotoVob` @0x00685580.

`EV_GotoVob` stores the target vob (field +0x4d4), copies the vob's world position into the
follow-target slot (fields +0x4c8/+0x4cc/+0x4d0), sets a per-tick obstacle-check countdown
(field +0x4ec = `500.0`), and crucially seeds the **arrival threshold** field +0x4e4 with the
constant `40000.0` (`0x471c4000`). It then calls `oCNpc::RbtGotoFollowPosition` @0x00688450, which
re-reads the target vob's current position every tick (moving-target follow) and calls
`oCNpc::RobustTrace` @0x00686960.

In `RobustTrace`, the current squared distance to the follow position is computed via
`zVEC3::Length2` (squared magnitude, stored in field +0x4dc) and the "target reached" bit (field
+0x4c4 bit 0) is set when `dist² (+0x4dc) < threshold (+0x4e4)`. With the final-target threshold
`40000.0`, that is a stop distance of `sqrt(40000) = 200` units. (For the intermediate
obstacle-avoidance sub-waypoints, `RbtGotoFollowPosition` lowers +0x4e4 to `6400.0` = 80 units, but
the final approach distance to the followed NPC is the fixed **200**.)

So in the original engine, `AI_GotoNpc` / companion-follow halts at a **fixed 200-unit** center-to-
center distance from the target, independent of weapon or guild.

## OpenGothic file:line

`game/world/objects/npc.cpp:1521-1533` (`Npc::implGoTo(uint64_t dt)`):

```cpp
bool Npc::implGoTo(uint64_t dt) {
  float dist = 0;
  if(go2.npc) {
    dist = fghAlgo.prefferedAttackDistance(*this,*go2.npc,owner.script());
    } else {
    ...
```

`AI_GoToNpc` is handled at `npc.cpp:2595` (`go2.set(act.target)` -> flag `GT_Way`, `go2.npc` set).
For a non-enemy NPC follow target, `GoTo::isClose` (`npc.cpp:52-60`) falls through to
`MoveAlgo::isClose(self,*npc,dist)` (`movealgo.cpp:704-707`), i.e. `qDistTo < dist*dist`, using the
`dist` computed above.

## Divergence

OpenGothic uses `FightAlgo::prefferedAttackDistance` (`fightalgo.cpp:286-291`) =
`fight_range_base[target] + fight_range_base[self] + weaponRange(self)` as the AI_GotoNpc stop
distance. This is a **combat** metric that scales with the equipped weapon's reach and per-guild
fight ranges. The original uses a single hard-coded **200-unit** arrival radius for GotoVob/follow
that has nothing to do with weapons or guild. Consequences: a companion following the player stops
at a weapon/guild-dependent distance (too far when carrying a 2H weapon, potentially too close when
unarmed) instead of the canonical fixed 200.

Note: the `go2.npc` branch in `implGoTo` is only exercised by `AI_GotoNpc` (flag `GT_Way`).
Enemy approach uses flag `GT_Enemy`, which `GoTo::isClose` short-circuits via `isInWRange` and
ignores this `dist` entirely (`npc.cpp:53-54`), so the fight-range value here serves no enemy path.

## Proposed patch

OLD (`game/world/objects/npc.cpp`, `Npc::implGoTo(uint64_t dt)`):
```cpp
  if(go2.npc) {
    dist = fghAlgo.prefferedAttackDistance(*this,*go2.npc,owner.script());
    } else {
```

NEW:
```cpp
  if(go2.npc) {
    // NOTE: in original-game oCNpc::EV_GotoVob @0x00685580 / RobustTrace @0x00686960 the
    // AI_GotoNpc (oCMsgMovement sub-type 2) follow arrival radius is the fixed engine constant
    // 200 units (reached when dist^2 < 40000.0); it does NOT use fight/weapon range. Enemy
    // approach uses GT_Enemy + isInWRange, so this branch is the AI_GotoNpc path only.
    dist = 200.f;
    } else {
```

`prefferedAttackDistance`, `go2.npc`, `GoTo::isClose`, `MoveAlgo::isClose(const Npc&,const Npc&,float)`,
and the `GT_Way`/`GT_Enemy` hints are all grep-verified to exist (`fightalgo.cpp:286`,
`npc.h:458/470`, `npc.cpp:52`, `movealgo.cpp:704`, `npc.h:29-37`).
