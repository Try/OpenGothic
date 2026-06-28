# Footstep quiet-sound perception: animation-event cadence vs original time-throttle

**Confidence:** Medium (divergence verified against the decompile; fix is structural, so DEFERRED)

## Original fn + address
`oCAIHuman::CreateFootStepSound` @ `0x0069b180` is the only producer of the player's
walking quiet-sound perception. It is **not** driven by footstep animation events.
Its sole caller is the player action handler `oCAIHuman` PC-action `FUN_0069ae20`
@ `0x0069ae20` (oAiHuman.cpp), which runs once per AI tick and *always* invokes
`CreateFootStepSound(this, 0)` (constant foot argument 0) at the very end.

Inside `CreateFootStepSound`, when the AI water-mode field (`this+0x160`) is not the
dive value and `oCAniCtrl_Human::IsWalking()` is true, it advances a single static
time accumulator (`_DAT_00aad6b4 += DAT_0099b3d8`, the per-tick increment) and only
when it crosses a fixed threshold (`_DAT_0083be20`) does it subtract the threshold and
call `oCNpc::AssessQuietSound_S` (which broadcasts the quiet-sound perception that other
NPCs assess). The cadence is therefore a **fixed wall-clock-style interval while
walking**, fully decoupled from which animation frame / which foot is planted. (When
`CreateFootStepSound` is called with a non-zero foot arg the accumulator is reset to 0;
the player path never does this.)

## OG file:line
- `game/world/objects/npc.cpp:2489-2491` (`Npc::tickAnimationTags`)
- `game/graphics/mesh/animation.cpp:452-456` (`ev.groundSounds++` per `d.gfx` SFXGrnd event)

## Divergence
OpenGothic ties `PERC_ASSESSQUIETSOUND` to footstep **animation events**: every tick it
counts the SFXGrnd ground events that fired (`ev.groundSounds`) and raises one
perception if `ev.groundSounds>0`. Consequences vs the original:

1. **Cadence is animation-driven, not time-throttled.** Each foot-plant SFXGrnd frame
   raises a perception, so a normal walk cycle emits ~one per foot (≈2× per stride),
   and the rate scales with animation/movement speed. The original emits at a single
   fixed time interval (`_DAT_0083be20`) regardless of stride speed or foot.
2. **No throttle / no shared accumulator.** The original gates on the static
   accumulator, capping how often the player can be "heard"; OpenGothic has no such cap.
3. **Different trigger domain.** The original keys off `IsWalking()` per tick; OpenGothic
   keys off the presence of an SFXGrnd tag this tick, so walking states/animations
   that lack SFXGrnd frames would not raise a perception (and vice-versa).

Net effect: the player broadcasts quiet-sound perceptions more often and at an
animation-dependent rate, making sneak-detection by guards somewhat more sensitive and
less deterministic than in `Gothic2.exe`.

Note: the audible footstep path itself is faithful — the material-group string table
(`MaterialGroupNames[]` UNDEF/METAL/STONE/WOOD/EARTH/WATER/SNOW, default UNDEF) matches
`zCMaterial::GetMatGroupString` exactly, and `Npc::emitSoundGround` builds
`<event>_<MATGROUP>` as the original does. The already-noted air/swim/dive and WM_Sneak
gating at npc.cpp:2489 is correct; only the *cadence/throttle* of the perception differs.

## Proposed patch
DEFERRED.

Reason: a faithful fix is not surgical. It requires decoupling the perception from
`ev.groundSounds` and instead running a per-player time accumulator each tick while
"walking" (mirroring `CreateFootStepSound`'s `_DAT_00aad6b4`/`DAT_0099b3d8`/
`_DAT_0083be20`), firing `PERC_ASSESSQUIETSOUND` only when the accumulator crosses the
threshold. The two original constants (per-tick increment `DAT_0099b3d8` and interval
threshold `_DAT_0083be20`, both floats in `.data`) could not be read out of the warm
decompiler (data reads unsupported), so their exact values are unknown — implementing
the throttle without them would be guesswork. Changing the trigger domain also risks
regressing the carefully-built #639 / air-swim-dive gate already in place. Holding for a
dedicated change once the two interval constants are recovered.

// NOTE: in original-game oCAIHuman::CreateFootStepSound @0x0069b180 (called once per
// tick from PC-action FUN_0069ae20 @0x0069ae20 with foot arg 0) the player's quiet-sound
// footstep perception is raised on a fixed static time interval (_DAT_0083be20) while
// IsWalking(), NOT once per SFXGrnd animation event as OpenGothic does at
// npc.cpp:2489 / animation.cpp:455.
