# Footstep GFX ground-sound gate fires while airborne / in water

**Confidence:** Medium

## Original function + address (prose only)

In `Gothic2.exe` the footstep activity for an NPC is driven by
`oCAIHuman::CreateFootStepSound` (entry `0x0069b180`, `oAiHuman.cpp`). That routine
only does anything when two conditions hold:

1. the AI's water/swim mode field (`this+0x160`) is **not** value 2 (the
   diving/under-water mode), and
2. `oCAniCtrl_Human::IsWalking()` returns true.

Only then does it advance the footstep accumulator and raise the quiet-sound
perception (`oCNpc::AssessQuietSound_S`). In other words, the original ties footstep
emission to the NPC actually walking on the ground and explicitly suppresses it while
the AI is in the water/dive mode. The per-material ground sound name is built from
`zCMaterial::GetMatGroupString` (entry `0x00565170`) whose switch maps groups 1..6 to
`METAL/STONE/WOOD/EARTH/WATER/SNOW` and default to `UNDEF` — this part already matches
OpenGothic's `MaterialGroupNames` (`game/game/constants.h:506`), so the divergence is
in the *gating*, not the name mapping.

## OpenGothic file:line

`game/graphics/mesh/animation.cpp:373` (`Animation::Sequence::processSfx`), the GFX
(ground-material footstep) emission block:

```cpp
if(npc!=nullptr && !npc->isInAir()) {
  for(auto& i:d.gfx){
    ...
    npc->emitSoundGround(i.name, i.range, i.empty_slot);
```

## Divergence

`Npc::isInAir()` (`game/world/objects/npc.cpp:1100`) returns true **only** for the
`MoveAlgo::InAir` state. The `MoveAlgo::State` enum (`game/game/movealgo.h:37`) has
several other non-grounded states the gate does not cover: `Falling`, `Jump`,
`JumpUp`, `ClimbUp`, `Swim`, and `Dive`. As a result OpenGothic will still emit GFX
ground footstep sounds while the NPC is falling, in the middle of a jump arc, climbing
a ledge, or — most audibly wrong — while swimming or diving underwater. During
`Swim`/`Dive`, `MoveAlgo::groundMaterial()` (`game/game/movealgo.cpp:1072`) returns
`WATER`, so the events come out as `<gfx>_WATER` even when the NPC is deep underwater
(`Dive`), whereas the original's `CreateFootStepSound` suppresses all footstep activity
in water-mode 2.

The clearly-wrong, audible case is the water path (`Swim`/`Dive`): the original gates
this off via the water-mode-2 check; OpenGothic does not. The airborne states
(`Jump`/`JumpUp`/`Falling`/`ClimbUp`) are a weaker case because the corresponding jump
/climb animations may not carry GFX events, so they are folded in conservatively.

## Proposed patch

Broaden the GFX gate so footstep ground sounds only play when the NPC is actually
grounded (not airborne and not in water). All predicates below are grep-verified to
exist on `Npc` (`game/world/objects/npc.h:192,196,198`).

OLD (`game/graphics/mesh/animation.cpp:373`):
```cpp
  if(npc!=nullptr && !npc->isInAir()) {
```

NEW:
```cpp
  // NOTE: in original-game oCAIHuman::CreateFootStepSound @0x0069b180 footstep
  // activity is only raised while IsWalking() is true and the AI water-mode field
  // (this+0x160) is not the dive/swim value (2); i.e. no ground footstep sounds while
  // airborne or in water. Npc::isInAir() alone only covers MoveAlgo::InAir, leaving
  // Jump/JumpUp/Falling/Swim/Dive uncovered (movealgo.h:37), so swim/dive anims would
  // wrongly emit <gfx>_WATER footsteps.
  if(npc!=nullptr && !npc->isInAir() && !npc->isSwim() && !npc->isDive()) {
```

If broader airborne suppression is desired, also `&& !npc->isJump() && !npc->isJumpUp()
&& !npc->isFalling()` (all grep-verified at `npc.cpp:1088/1104/1108`); deferred from the
minimal patch because it is unconfirmed that jump/fall animations carry GFX events.
