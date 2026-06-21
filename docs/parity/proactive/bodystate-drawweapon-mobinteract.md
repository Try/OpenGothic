# Body-state: weapon draw blocked during MOBSI interaction

**Confidence:** Medium

## Original function + address

`oCNpc::CanDrawWeapon` (Gothic2.exe `0x006805c0`). The original allows
drawing/switching a weapon when ANY of the following holds:

1. the ani-controller reports `IsStanding()` (covers the standing pose), OR
2. it reports `IsWalking()` (covers walk / run / sneak), OR
3. **the NPC is currently in a mob interaction** (`GetInteractMob() != NULL`), OR
4. the current weapon mode is already magic (5) or ranged/bow (6).

If none of these hold it returns 0. Branch 3 is the relevant one: the original
explicitly permits a weapon draw while the NPC is engaged with a MOBSI, so a
player at a forge / workbench / sitting MOBSI can press a weapon key and the
draw proceeds (interrupting the interaction).

## OpenGothic file:line

`game/world/objects/npc.cpp:3658` (`Npc::canSwitchWeapon`)

```
auto bs = bodyStateMasked();
if(bs==BS_STAND || bs==BS_WALK || bs==BS_RUN || bs==BS_SNEAK || bs==BS_NONE)
  return true;
return false;
```

This allow-list covers stand/walk/run/sneak (matching original branches 1+2)
but has no equivalent of branch 3: `BS_MOBINTERACT` (15) and
`BS_MOBINTERACT_INTERRUPT` (16) are absent, so `canSwitchWeapon()` returns
false during any mob interaction. Every draw path
(`drawWeaponMelee`/`drawWeaponBow`/`drawMage`, and the `AI_DrawWeapon*`
commands at npc.cpp:2702-2721) checks `canSwitchWeapon()` first and bails.
For the player, `PlayerControl::implMove` (game/game/playercontrol.cpp:625)
routes to `implMoveMobsi` and returns before the weapon-control block, so the
only way out of a MOBSI is the Back key.

## Divergence

In the original, drawing a weapon while at a MOBSI is permitted and cancels the
interaction. In OpenGothic the weapon-draw input/command is silently ignored
until the NPC leaves the MOBSI by other means.

## Proposed patch

```cpp
// game/world/objects/npc.cpp  Npc::canSwitchWeapon()
// OLD
bool Npc::canSwitchWeapon() const {
  if(isUnconscious())
    return false;
  auto bs = bodyStateMasked();
  if(bs==BS_STAND || bs==BS_WALK || bs==BS_RUN || bs==BS_SNEAK || bs==BS_NONE)
    return true;
  return false;
  }

// NEW
bool Npc::canSwitchWeapon() const {
  if(isUnconscious())
    return false;
  // NOTE: in original-game oCNpc::CanDrawWeapon (Gothic2.exe 0x006805c0) a weapon
  // draw is also allowed while in a mob interaction (GetInteractMob()!=NULL); the
  // draw then interrupts the MOBSI. OpenGothic omitted that branch.
  if(interactive()!=nullptr)
    return true;
  auto bs = bodyStateMasked();
  if(bs==BS_STAND || bs==BS_WALK || bs==BS_RUN || bs==BS_SNEAK || bs==BS_NONE)
    return true;
  return false;
  }
```

Note: the draw helpers already call `setInteraction(nullptr,true)` to leave the
MOBSI before playing the draw animation, so allowing the gate to pass while
`interactive()!=nullptr` lets that existing exit logic run. The player path in
`implMove` (which `return`s into `implMoveMobsi`) would additionally need to
forward weapon-draw keys for the player-facing case to take full effect.
