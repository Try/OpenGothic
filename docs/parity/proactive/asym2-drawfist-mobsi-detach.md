# Asymmetric draw path: drawWeaponFist does not leave the MOBSI the other three draw helpers exit

**Confidence:** Medium

## Original function + address (prose)

In the original, `oCNpc::CanDrawWeapon` (Gothic2.exe `0x006805c0`) explicitly
permits drawing/switching a weapon while the NPC is engaged with a MOBSI
(`GetInteractMob() != NULL`) — this is the same branch already cited in
OpenGothic's `Npc::canSwitchWeapon` (npc.cpp:3995-3998). When the draw message is
then processed, the weapon-draw transition ends the current mob interaction
(`oCNpc::Interrupt` @ `0x00735ab0` / EndInteraction) so the readied-stance
animation can play out of the workbench/forge/sitting pose. This interrupt is
driven by the *act of drawing*, not by which weapon mode (fist / 1H / 2H / bow /
crossbow / mage) is being drawn — the fist mode (WeaponMode 1) is treated the
same as a sword.

## OpenGothic file:line

`game/world/objects/npc.cpp:4040` — `Npc::drawWeaponFist()`.

OpenGothic models the "draw interrupts the MOBSI" behavior by calling
`setInteraction(nullptr,true)` (detach from the current interactive) right before
playing the ready animation. This call is present in **all three armed** draw
helpers:

- `drawWeaponMelee()` — npc.cpp:4077 (`if(!setInteraction(nullptr,true)) return false;`)
- `drawWeaponBow()`   — npc.cpp:4101 (same)
- `drawSpell()`       — npc.cpp:4133 (same)

but is **absent** from `drawWeaponFist()` (npc.cpp:4040-4062), which goes straight
from the `closeWeapon` early-return to `visual.startAnim(*this,WeaponState::Fist)`.

## Divergence

`drawWeaponFist()` is reachable while in a MOBSI: `canSwitchWeapon()` returns
`true` during an interaction (the deliberate mobsi-draw allowance), and the fist
path is taken whenever an **unarmed** actor draws "melee":

- Player: `PlayerControl::implMoveMobsi`/weapon block routes the melee key to
  `pl.drawWeaponFist()` when `currentMeleeWeapon()==nullptr`
  (game/game/playercontrol.cpp:643-645).
- AI / fallback: `drawWeaponMelee()` falls back to `return drawWeaponFist();`
  when there is no melee weapon (npc.cpp:4070-4071) — this fallback happens
  *before* `drawWeaponMelee`'s own `setInteraction` at 4077, so the detach never
  runs on the fist branch.

Result: an **armed** actor at a forge/workbench/bench who presses draw leaves the
MOBSI and readies the weapon (matches the original); an **unarmed** actor in the
same spot plays the fist-ready animation while still attached to the MOBSI — a
state the original never produces, since the draw uniformly ends the interaction
regardless of weapon mode. The two parallel paths diverge only by the missing
detach.

For non-interacting actors the added call is a guaranteed no-op:
`setInteraction(nullptr,true)` returns `true` immediately when
`currentInteract==nullptr` (npc.cpp:4708-4709), so monsters and free-standing
NPCs are unaffected — only the in-MOBSI fist-draw is corrected.

## Proposed patch

```cpp
// game/world/objects/npc.cpp  Npc::drawWeaponFist()
// OLD
  if(weaponSt!=WeaponState::NoWeapon) {
    closeWeapon(false);
    return false;
    }

  if(isMonster()) {

// NEW
  if(weaponSt!=WeaponState::NoWeapon) {
    closeWeapon(false);
    return false;
    }

  // NOTE: in original-game oCNpc::CanDrawWeapon @0x006805c0 permits a weapon draw during a mob
  // interaction (GetInteractMob()!=NULL) and the draw transition then ends that interaction
  // (oCNpc::Interrupt @0x00735ab0) independently of the target weapon mode (fist included).
  // The armed draw helpers mirror this with setInteraction(nullptr,true) (drawWeaponMelee:4077,
  // drawWeaponBow:4101, drawSpell:4133); drawWeaponFist omitted it, so an unarmed actor that drew
  // "melee" (player melee key / drawWeaponMelee no-weapon fallback) readied fists while still
  // attached to the MOBSI. setInteraction(nullptr,true) is a no-op when not interacting.
  if(!setInteraction(nullptr,true))
    return false;

  if(isMonster()) {
```
