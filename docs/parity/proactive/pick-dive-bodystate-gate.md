# Pickup gating allows taking world items while diving (BS_DIVE)

**Confidence:** Medium-High

## Original fn + address

The world item-take in the original is driven by the animation-event handler
`oCNpc::EV_TakeVob` (@0x007534e0), which fires when the bend-down take animation
reaches its grab frame and is responsible for actually transferring the focused
vob into the inventory. The very first thing this handler does is query
`oCAniCtrl_Human::GetWaterLevel` (@0x006b89d0) and, **when the water level equals 2
(fully submerged / diving)**, it releases the pending take-target reference,
nulls it out, and returns without performing the take. `GetWaterLevel` returns
`2` only when the swim-state field is in the "dive" mode (field at +0x88 == 2)
and the model collision object is below the water surface; it returns `1` for
surface-swimming. The grab branch that follows (the `oCItem`-inheritance check
choosing `T_STAND_2_IGET`) is therefore unreachable while diving.

Net original behavior: a player/NPC can take an item from the world while
standing, sneaking, or surface-swimming, but the take is **aborted while diving
underwater** (water level 2). Surface-swim (level 1) is not aborted.

## OG file:line

`/Users/admin/Downloads/opengothic/game/world/objects/npc.cpp:3670-3673`
(`Npc::takeItem`):

```cpp
auto state = bodyStateMasked();
if(state!=BS_STAND && state!=BS_SNEAK && state!=BS_SWIM && state!=BS_DIVE) {
  return nullptr;
  }
```

`BS_DIVE` is included in the allow-list, so OpenGothic permits picking world
items up while fully submerged. `BS_DIVE` (==7) is defined in
`game/game/constants.h:172`; `BS_SWIM` is the surface-swim state. `setAnimAngGet`
(npc.cpp:1020) maps diving onto `WM_Dive`, but `AnimationSolver::solveAnim`
returns the same `S_IGET` regardless of walk-mode, so OG plays the normal item-
get animation underwater rather than refusing the take.

## Divergence

The original aborts a world item take when the actor is diving (water level 2);
OpenGothic's `Npc::takeItem` explicitly allows `BS_DIVE` and completes the take.
A player diving onto a focused item will pocket it in OpenGothic, whereas the
original game silently refuses the pickup until the actor is at most surface-
swimming. Surface-swim (`BS_SWIM`) is allowed in both and should stay.

## Proposed patch

```cpp
// OLD
auto state = bodyStateMasked();
if(state!=BS_STAND && state!=BS_SNEAK && state!=BS_SWIM && state!=BS_DIVE) {
  return nullptr;
  }

// NEW
// NOTE: in original-game oCNpc::EV_TakeVob @0x007534e0 the take is aborted when
// oCAniCtrl_Human::GetWaterLevel @0x006b89d0 returns 2 (fully submerged/diving),
// so a world item cannot be picked up while diving; surface-swim (level 1) is OK.
auto state = bodyStateMasked();
if(state!=BS_STAND && state!=BS_SNEAK && state!=BS_SWIM) {
  return nullptr;
  }
```

Caveat: confidence is held below "High" only because gameplay intent for any
underwater-retrievable items was not exhaustively cross-checked; the binary path
itself unambiguously aborts the take at water level 2.
