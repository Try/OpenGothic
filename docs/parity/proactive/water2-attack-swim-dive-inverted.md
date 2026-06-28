# Water gate inverted for attacks: `Npc::doAttack` blocks swim / allows dive (original is the opposite)

**Confidence:** High

## Original fn + address

`oCNpc::ThinkNextFightAction` @ `0x0067e37e` is the fight-think entry that decides every melee/fist
fight action for both AI npcs and the player. Right after caching the ani-controller it reads
`oCAniCtrl_Human::GetWaterLevel` @ `0x006b89d0` and tests `if (waterLevel > 1)` — i.e. only when the
water level is exactly `2` (diving). On that branch it skips `FindNextFightAction` entirely: it does
not start any attack, it just re-targets the enemy (`RbtUpdate`/`RobustTrace`) and returns 0. At
water level `0` (dry) or `1` (surface swimming) it falls through and computes/starts the fight
action normally. `oCNpc::Fighting` @ `0x006800ee` (the per-tick fight driver that calls
ThinkNextFightAction) repeats the same `GetWaterLevel > 1` short-circuit before it would otherwise
ready a weapon. So the original gate is unambiguous: **attacking is BLOCKED only while diving
(level 2) and ALLOWED while surface-swimming (level 1)**. This matches the sibling
`oCNpc::EV_DrawWeapon` @ `0x0074cc34`, which likewise blocks only `GetWaterLevel == 2` and permits
level 1 — both functions agree that level-1 surface swim is a permitted combat state and only level-2
dive is forbidden. `GetWaterLevel` returns `1` when the controller's water-state field is `1` (swim)
and `2` when it is `2` (dive); these map exactly onto OpenGothic's `mvAlgo.isSwim()` and
`mvAlgo.isDive()` (see the existing note at `npc.cpp:2180`).

## OG file:line

`game/world/objects/npc.cpp:4116` (inside `Npc::doAttack`).

```
bool Npc::doAttack(Anim anim, BodyState bs) {
  auto weaponSt = weaponState();
  if(weaponSt==WeaponState::NoWeapon || weaponSt==WeaponState::Mage)
    return false;

  if(mvAlgo.isSwim())          // <-- water gate
    return false;
  ...
  auto wlk = walkMode();
  if(mvAlgo.isInWater())
    wlk = WalkBit::WM_Water;
```

`doAttack` is the single consolidated attack executor reached by both the player
(`fistShoot`/`swingSword`/`swingSwordL`/`swingSwordR`/`finishingMove`) and the AI fight loop
(`processFight`, `npc.cpp:1787`), so it is OpenGothic's analogue of the original's
ThinkNextFightAction water gate.

## Divergence

The gate is **inverted** versus the original on both ends of the water range:

* While **surface swimming** (`isSwim()`, level 1) OpenGothic returns `false` and refuses the attack,
  but the original explicitly **allows** combat at level 1.
* While **diving** (`isDive()`, level 2) OpenGothic falls through and starts the attack (and, because
  `isInWater()` is then true, even selects the `WM_Water` combo variant at line 4123-4124), but the
  original **blocks** all fight actions at level 2.

So OpenGothic lets an npc swing/punch while fully submerged and forbids it at the surface — the exact
opposite of `ThinkNextFightAction`/`EV_DrawWeapon`. Because `isSwim()` and `isDive()` are mutually
exclusive MoveAlgo states in OpenGothic, swapping the predicate fixes both ends at once. (The
`wlk = WM_Water` branch at 4123 was effectively dead for the swim case and only ever applied to the
wrongly-permitted dive case; after the fix it correctly serves the now-permitted swim attack.)

## Proposed patch

```cpp
// OLD
  if(mvAlgo.isSwim())
    return false;

// NEW
  // NOTE: in original-game oCNpc::ThinkNextFightAction @0x0067e37e (mirrored by oCNpc::Fighting
  // @0x006800ee and oCNpc::EV_DrawWeapon @0x0074cc34) a fight action is refused only when
  // oCAniCtrl_Human::GetWaterLevel @0x006b89d0 returns 2 (diving); surface swimming (level 1) is a
  // permitted combat state. OpenGothic's isSwim()==level1 and isDive()==level2, so gate on isDive().
  if(mvAlgo.isDive())
    return false;
```
