# Water action-gating: fist weapon-mode not auto-sheathed on entering water

**Confidence:** Medium

## Original function + address

The auto-sheathe-on-entering-water lives in `oCAniCtrl_Human::CheckWaterLevel`
(Gothic2.exe `0x006ab130`). When the controller transitions a living NPC into the
swim action mode, it evaluates the guard

> `( GetWeaponMode() != 0  ||  HasTorch() )  &&  vtbl[0x10c]() == 0`

and, when true, posts an `oCMsgWeapon` of subtype **7 = EV_FORCEREMOVEWEAPON** to the
NPC and returns early (the swim animation only begins on a later frame once the weapon
is gone). `oCNpc::GetWeaponMode` (`0x00738c40`) returns the fmode field clamped to
`0..7`, where `1` is the bare-fist fighting mode. The condition is therefore
`weaponMode != 0`, which **includes fist mode (1)**.

`oCNpc::EV_ForceRemoveWeapon` (`0x0074ec40`) is what the message runs: it puts away a
held torch (re-creating `ItLsTorch` into the inventory) and calls the weapon-mode
setter `vtbl[0xa0](0)`, i.e. it drops the NPC back to weapon mode 0 regardless of which
non-zero mode it was in — bare fists included. (The `vtbl[0x10c]` guard, shared with
`EV_ForceRemoveWeapon`, is a movement/transform lock that is normally false for the
player.)

Net original behavior: raising fists and then swimming forces the fists down (weapon
mode → none), exactly as it does for any drawn melee/ranged/magic weapon.

## OpenGothic file:line

`game/game/movealgo.cpp:817-822` (`MoveAlgo::setState`, the `f==Swim` transition):

```cpp
if((f==Swim) && !(flags==Swim)) {
  auto ws = npc.weaponState();
  npc.setAnim(Npc::Anim::NoAnim);
  if(ws!=WeaponState::NoWeapon && ws!=WeaponState::Fist)   // <-- excludes Fist
    npc.closeWeapon(true);
  npc.dropTorch(true);
  }
```

## Divergence

OpenGothic auto-sheathes every weapon state on entering Swim **except `Fist`**. The
original removes the weapon for any non-zero weapon mode, fist (mode 1) included.
Consequence: in OpenGothic, a player (or NPC) who enters water with fists raised keeps
`WeaponState::Fist` while swimming and after surfacing. Because
`PlayerControl::canInteract()` (`game/game/playercontrol.cpp:434`) returns false while
`weaponState()!=WeaponState::NoWeapon`, the fists-up swimmer is also blocked from
ransacking/talking/using mobs in the water — interactions the original permits after it
lowers the fists. The fist fighting overlay likewise persists over the swim animation
instead of being cleared.

`MoveAlgo::isSwim`/`isDive` gate Swim only here; entering Dive in normal play always
passes through Swim first (the only `setState(Swim)` site is the chest-deep branch in
`MoveAlgo::tickWater`), so this single transition is the auto-sheathe point.

## Proposed patch

Match the original `weaponMode != 0` condition by dropping the `Fist` exception.
`Npc::closeWeapon(true)` already handles `WeaponState::Fist` (no-anim path:
`setToFightMode(NoWeapon)` + slot reset), so it is the correct force-remove analogue.

OLD (`game/game/movealgo.cpp:820-821`):
```cpp
    if(ws!=WeaponState::NoWeapon && ws!=WeaponState::Fist)
      npc.closeWeapon(true);
```
NEW:
```cpp
    // NOTE: in original-game oCAniCtrl_Human::CheckWaterLevel @0x006ab130 the swim
    // transition force-removes the weapon whenever GetWeaponMode()!=0 (EV_FORCEREMOVEWEAPON,
    // oCNpc::EV_ForceRemoveWeapon @0x0074ec40). Weapon mode 1 is bare fists, so fists are
    // sheathed on entering water too; do not special-case WeaponState::Fist.
    if(ws!=WeaponState::NoWeapon)
      npc.closeWeapon(true);
```

Grep-verified OG symbols: `WeaponState::{NoWeapon,Fist}` (`game/game/constants.h:201-209`),
`Npc::weaponState()`/`Npc::closeWeapon(bool)`/`Npc::dropTorch(bool)`
(`game/world/objects/npc.cpp:3768,3920,847`), `PlayerControl::canInteract()`
(`game/game/playercontrol.cpp:434`).
