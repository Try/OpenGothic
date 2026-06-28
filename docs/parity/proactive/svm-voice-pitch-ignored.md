# SVM/voice parity: `C_NPC.voice_pitch` is never applied to NPC voice sounds

**Confidence:** Medium (divergence is certain in code; stock-gameplay magnitude depends on how
many NPC instances set a non-zero `voice_pitch`. Fix is DEFERRED because the Tempest audio
backend exposes no per-source pitch control.)

## Original function + address (prose)

`oCNpc::OnDamage_Sound` (`Gothic2.exe @0x0067a8a0`) builds the hurt/death line
`SVM_<voice>_AARGH[_v]` / `SVM_<voice>_DEAD` and then plays it through
`zsound->PlaySound3D(...)` (`zCSndSys_MSS::PlaySound3D @0x004f10f0`, the
`zCSoundFX*` overload). It fills a `zCSoundSystem::zTSound3DParams` on the stack and sets the
last field, `zTSound3DParams +0x1c` (the *pitch offset*), to `(float)*(int*)(this+0x258)`.
That field is `C_NPC.voice_pitch`: the SVM voice index is `oCNpc +0x254` (used one line earlier to
assemble `"SVM_"+voice+...`), and `voice_pitch` is the immediately-following int (`+0x258`), exactly
mirroring the script layout (`zenkit::INpc::voice` then `voice_pitch`, daedalus.hh:201-202).

Inside `PlaySound3D` the value at `params+0x1c` is copied into the sound frame at `+0x14` and then,
when it is non-zero and not the `0xc97423f0` "no-pitch" sentinel, run through the same
`pow()`+`ftol()` conversion as `zCSndFrame::CalcPitchOffset` (`@`, `zSndMss.cpp`) to produce the
playback-frequency ratio stored at frame `+0x60`. In other words `voice_pitch` is an exponential
(semitone-style) pitch shift applied to the NPC's spoken/hurt voice; `0` means "leave pitch
unchanged".

## OG file:line

- `game/world/objects/npc.cpp:3298` `Npc::emitSoundEffect` — plays the SVM via `::Sound(... range ...)`
  with no pitch argument.
- `game/world/objects/npc.cpp:3310-3332` `Npc::emitSoundSVM` — assembles `SVM_<voice>_AARGH/_DEAD`
  but uses only `hnpc->voice`; `hnpc->voice_pitch` is never read.
- `game/game/gamescript.cpp:1315-1340` `aiOutput` / `aiOutputSvm` — the dialog/SVM voice path; calls
  `world().addDlgSound(...)`, also pitch-unaware.
- `game/world/worldsound.cpp:118` `addDlgSound` / `game/world/objects/sound.h:5-43` `Sound` /
  `lib/Tempest/Engine/sound/soundeffect.h:25-60` `Tempest::SoundEffect` — none expose a pitch/
  frequency setter. `voice_pitch` is read and round-tripped through save games
  (`game/game/serialize.cpp:300,324`) but is otherwise unused.

## Divergence

The original shifts every NPC voice sample by `C_NPC.voice_pitch` (AARGH/DEAD via
`OnDamage_Sound`, and the same `zTSound3DParams` pitch field is the engine's general voice-play
path). OpenGothic loads, stores and serializes `voice_pitch` but applies it to nothing: every voice
line plays at the sample's native pitch. For any NPC whose script sets a non-zero `voice_pitch`, the
spoken pitch is wrong (too high/too low relative to the original).

## Proposed patch — DEFERRED

A faithful fix needs three pieces that don't exist yet:

1. `Tempest::SoundEffect` must gain a `setPitch(float ratio)` (OpenAL `AL_PITCH` on the source);
   today it has only `setVolume`/`setMaxDistance`/position.
2. `Npc::emitSoundEffect` / `Sound` must forward a pitch ratio.
3. `emitSoundSVM` (and the `aiOutput`/`addDlgSound` dialog path) must compute the ratio from
   `hnpc->voice_pitch` using the original's exponential mapping
   (`ratio = pow(base, voice_pitch)`, guarding `voice_pitch==0` → ratio 1.0, matching the
   `0xc97423f0` sentinel skip in `zCSndFrame::CalcPitchOffset`). The exact `pow` base should be read
   off `CalcPitchOffset`'s embedded constant before committing the formula.

Because step 1 is an engine-level (Tempest/OpenAL) change rather than a surgical game-logic edit, and
because the stock-content magnitude is unverified, this is filed as DEFERRED rather than patched.

```
// NOTE: in original-game oCNpc::OnDamage_Sound @0x0067a8a0 the voice line is played via
// zCSndSys_MSS::PlaySound3D @0x004f10f0 with zTSound3DParams.pitchOffset = (float)C_NPC.voice_pitch
// (oCNpc+0x258, the int right after voice at +0x254). PlaySound3D converts it through the same
// pow()-based ratio as zCSndFrame::CalcPitchOffset (sentinel 0xc97423f0 == "no change", 0 == native
// pitch). OpenGothic serializes voice_pitch but never feeds it to the sound, so pitched NPC voices
// play at native pitch. Fix requires Tempest::SoundEffect pitch support (AL_PITCH) -> DEFERRED.
```
