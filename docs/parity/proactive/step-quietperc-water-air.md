# Footstep quiet-sound perception fires while swimming/diving/in-air

**Confidence:** High

## Original fn + address

`oCAIHuman::CreateFootStepSound` @ `0x0069b180` is the only producer of the
"quiet sound" footstep perception. Its body gates the work behind two
conditions before it ever reaches `oCNpc::AssessQuietSound_S` (the perception
broadcast):

1. the AI water-mode field (`oCAIHuman+0x160`) must **not** equal `2` (the
   dive / fully-submerged value), and
2. `oCAniCtrl_Human::IsWalking()` must be true.

`IsWalking()` is true only for actual ground locomotion -- it is false while
airborne (jump/fall) and false while swimming on the surface. Combined with the
`!= 2` dive guard, the original therefore emits the quiet-sound footstep
perception **only when the NPC is walking/running on the ground**, never while
in the air, swimming, or diving.

## OG file:line

`/Users/admin/Downloads/opengothic/game/world/objects/npc.cpp:2466`
(`Npc::tickAnimationTags`)

The `groundSounds` count it tests is incremented unconditionally in
`Animation::Sequence::processEvents`
(`/Users/admin/Downloads/opengothic/game/graphics/mesh/animation.cpp:455`),
with **no** water/air gate.

## Divergence

OpenGothic raises `PERC_ASSESSQUIETSOUND` whenever a gfx ground-event lands in
the frame window (`ev.groundSounds>0`), gated only on `isPlayer()` and the
non-sneak walk-flag. It is **not** gated on the locomotion/water state.

The already-applied audible-footstep fix (animation.cpp:377, NOTE citing
`oCAIHuman::CreateFootStepSound`) explicitly documents that swim/dive animations
DO carry gfx ground-events -- they "would otherwise emit `<gfx>_WATER`
footsteps even deep underwater." That same event stream still increments
`ev.groundSounds`, so a swimming or diving player keeps broadcasting the silent
footstep perception (waking/alerting nearby NPCs) in exactly the states where
the original suppresses it. The audible footstep was silenced in water/air but
its perception twin was left ungated, an internal inconsistency versus the
original's single `IsWalking() && water-mode != dive` gate.

## Proposed patch

OLD (`game/world/objects/npc.cpp:2462-2467`):
```cpp
  // NOTE: in original-game the quiet-sound footstep perception is suppressed by the
  // persistent sneak walk-mode, not the transient body-state: during a mobsi interaction
  // (e.g. lock-picking) the body-state leaves BS_SNEAK, which wrongly let footstep sounds
  // wake nearby NPCs while still sneaking (#639). Key it off the WM_Sneak walk-flag.
  if(ev.groundSounds>0 && isPlayer() && (wlkMode&WalkBit::WM_Sneak)!=WalkBit::WM_Sneak)
    world().sendImmediatePerc(*this,*this,*this,PERC_ASSESSQUIETSOUND);
```

NEW:
```cpp
  // NOTE: in original-game the quiet-sound footstep perception is suppressed by the
  // persistent sneak walk-mode, not the transient body-state: during a mobsi interaction
  // (e.g. lock-picking) the body-state leaves BS_SNEAK, which wrongly let footstep sounds
  // wake nearby NPCs while still sneaking (#639). Key it off the WM_Sneak walk-flag.
  // NOTE: in original-game oCAIHuman::CreateFootStepSound @0x0069b180 the quiet-sound
  // perception (oCNpc::AssessQuietSound_S) only runs while IsWalking() and the AI water-mode
  // is not the dive value -- i.e. never airborne, swimming, or diving. The gfx ground-events
  // that drive ev.groundSounds still fire underwater (see animation.cpp processSfx NOTE), so
  // without this gate a swimming/diving player keeps broadcasting silent footstep perceptions.
  if(ev.groundSounds>0 && isPlayer() && (wlkMode&WalkBit::WM_Sneak)!=WalkBit::WM_Sneak &&
     !isInAir() && !isSwim() && !isDive())
    world().sendImmediatePerc(*this,*this,*this,PERC_ASSESSQUIETSOUND);
```

This mirrors the exact state-set already used by the audible footstep loop
(`!npc->isInAir() && !npc->isSwim() && !npc->isDive()` at animation.cpp:377),
keeping the audible footstep and its perception twin gated identically, as the
original's single function does.
