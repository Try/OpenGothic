# Anim-event SFX placed on the last frame re-fires every tick (non-looping anims)

**Confidence:** High (for the non-looping case the fix is strictly correct; the
looping case is left unchanged and out of scope).

## Original function + address

`zCModel::DoAniEvents` (zModel.cpp), at virtual address **0x0057b890** in
`Gothic2.exe`, is the per-frame animation-event dispatcher. It is invoked once
per `zCModel::AdvanceAni` step (the forward caller at **0x0057c819**, plus the
reverse-direction variant). DoAniEvents walks the active animation's event list
starting at the cursor stored in the `zCModelAniActive` object (member offset
+0x10) and, for each event, fires it only when the playback frame has crossed
the event's frame in the current step. The EVENT_SFX case (switch case 1) and the
EVENT_SFX_GRND case (switch case 2) just play the sound; there is **no** special
handling for events that sit on the animation's last frame.

Crucially, at the bottom of the dispatch loop the event cursor is advanced by the
playback direction: `*(param_1+0x10) += *(param_1+8)` (±1). Each event index is
therefore consumed exactly once and never revisited within a pass. The
consequence is that the original engine fires a given SFX event **exactly once**
when playback passes its frame — including an SFX whose frame equals the
animation's last frame, which fires once at the animation's end (and, for a true
looping animation, once per loop iteration because AdvanceAni resets the cursor on
wrap).

## OpenGothic file:line

`game/graphics/mesh/animation.cpp:359` (inside `Animation::Sequence::processSfx`).

## Divergence

The SFX firing predicate is:

```cpp
if(((frameA<=fr && fr<frameB) ^ invert) || i.frame==int32_t(d.lastFrame)) {
```

The second disjunct `i.frame==int32_t(d.lastFrame)` is unconditional: any SFX
event whose frame equals the animation's last frame fires on **every** tick the
animation is alive, not once. `processSfx` is called every animation update
(`MdlVisual::updateAnimation` -> `Pose::processSfx`, `game/graphics/mdlvisual.cpp:504`),
so a last-frame SFX is emitted repeatedly each frame instead of once. With
`empty_slot==true` (free-slot playback) this stacks/retriggers the sound audibly;
even with a fixed slot it diverges from the original's single emission.

For a **non-looping** animation the divergence is pure over-fire: `frameClamp`
maps a normal last-frame event to `numFrames-1`, and as `frameB` reaches
`numFrames` the ordinary window `frameA<=fr && fr<frameB` already fires it exactly
once. The special clause adds nothing but the spurious per-tick re-trigger.

(The clause was presumably added to cover **looping** animations, where
`extractFrames` takes `frameB%=numFrames`, so a last-frame event can fall outside
the wrapped `[frameA,frameB)` window and be missed. That looping case needs a
fire-once-per-iteration fix and is intentionally left untouched here.)

## Proposed patch

Restrict the last-frame clause to looping animations, so non-looping animations
match the original's fire-once semantics via the ordinary window, while loop
coverage is preserved unchanged.

OLD (`game/graphics/mesh/animation.cpp`, in `Animation::Sequence::processSfx`):
```cpp
  for(auto& i:d.sfx) {
    uint64_t fr = frameClamp(i.frame,d.firstFrame,d.numFrames,d.lastFrame);
    if(((frameA<=fr && fr<frameB) ^ invert) || i.frame==int32_t(d.lastFrame)) {
      if(npc!=nullptr)
        npc->emitSoundEffect(i.name,i.range,i.empty_slot);
      if(mob!=nullptr)
        mob->emitSoundEffect(i.name,i.range,i.empty_slot);
      }
    }
```

NEW:
```cpp
  for(auto& i:d.sfx) {
    uint64_t fr = frameClamp(i.frame,d.firstFrame,d.numFrames,d.lastFrame);
    // NOTE: in original-game zCModel::DoAniEvents @0x0057b890 each anim event
    // (incl. an SFX on the last frame) fires exactly once as the playback cursor
    // crosses its frame; the cursor (zCModelAniActive+0x10) is advanced once per
    // event and never revisited. For a non-looping anim the ordinary window
    // already fires a last-frame SFX once, so the last-frame fallback must only
    // cover the looping case (where frameB wraps modulo numFrames and could miss
    // the event); otherwise it re-fires every tick.
    if(((frameA<=fr && fr<frameB) ^ invert) ||
       (animCls==Animation::Loop && i.frame==int32_t(d.lastFrame))) {
      if(npc!=nullptr)
        npc->emitSoundEffect(i.name,i.range,i.empty_slot);
      if(mob!=nullptr)
        mob->emitSoundEffect(i.name,i.range,i.empty_slot);
      }
    }
```

Symbols verified against OG source: `animCls` and `Animation::Loop`
(`game/graphics/mesh/animation.h:16-19,109`); `MdsSoundEffect` fields `frame`,
`name`, `range`, `empty_slot`; `Sequence::data->{firstFrame,numFrames,lastFrame}`.
