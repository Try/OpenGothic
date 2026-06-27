# Parity: world ambient VobSound ignores the per-vob sndVolume (0-100%) scale

**Confidence:** High (that the original applies the scale; impact limited to VobSounds whose `sndVolume != 100`)

## Original function + address (prose only)
`zCVobSound::DoSoundUpdate` (Gothic2.exe @ 0x0063e210) drives each world VobSound. Once per
update it calls `zCVobSound::CalcVolumeScale` (@ 0x0063dde0) and feeds the result as the
`volume` field of the `zCSoundSystem::zTSound3DParams` struct passed to the active sound update
(`zCSndSys_MSS::UpdateSound3D` @ 0x004f2410, which clamps that scale to <= 1.0 and multiplies it
into the final sample volume via `zCActiveSnd::GetVolume`).

`CalcVolumeScale` begins by converting the vob's `sndVolume` property (stored 0..100) into a
0..1 fraction: it multiplies the volume field (object offset +0x14c) by the float constant
`0x3c23d70a` (= 0.00999999978 ≈ 1/100). When the sound is a directional cone vob the result is
further weighted, but the base term for every VobSound is `sndVolume / 100`. So the original
final playback volume = (SoundFX-definition volume) * (sndVolume / 100) * occlusion.

## OpenGothic file:line
- `game/world/worldsound.cpp:90` `WorldSound::addSound(const zenkit::VSound&)` — copies
  `vob.mode`, `vob.random_delay`, `vob.radius`, `vob.position`, the sound names, but never reads
  `vob.volume` (zenkit `VSound::volume`, the `sndVolume` 0-100% field, defined in
  `lib/ZenKit/include/zenkit/vobs/Sound.hh:50` and parsed at `lib/ZenKit/src/vobs/Sound.cc:13`).
- `game/world/worldsound.cpp:156` `implAddSound` sets `ex->vol = ex->eff.volume();` — i.e. only
  the SoundFX-definition volume (`SoundFx::SoundVar::vol = sfx.vol/127`, `game/sound/soundfx.cpp:13`).

A grep confirms `vob.volume` / the `VSound::volume` field is used nowhere in `game/` for sounds.

## Divergence
For world ambient/random VobSounds (the `worldEff` path: `addSound` -> `tick`), OpenGothic plays
the sound at the raw SoundFX-definition volume and omits the `* sndVolume/100` factor that the
original always applies. A VobSound authored with e.g. `sndVolume = 50` plays at full SoundFX
volume in OpenGothic but at half volume in the original. (Cone/`sndConeAngle` directional
weighting is a separate, larger gap and is intentionally out of scope here.)

## Proposed patch
Thread the vob volume fraction through `WSound` and apply it to each spawned instance.

OLD (`game/world/worldsound.cpp`, struct `WSound`):
```
  Tempest::Vec3  pos;
  float          sndRadius      = 2500;
```
NEW:
```
  Tempest::Vec3  pos;
  float          sndRadius      = 2500;
  float          vol            = 1.f; // sndVolume/100, see #snd3d-vobsound-volume-scale
```

OLD (`WorldSound::addSound`, after `s.sndRadius = vob.radius;`):
```
  s.pos       = {vob.position.x,vob.position.y,vob.position.z};
  s.sndRadius = vob.radius;
```
NEW:
```
  s.pos       = {vob.position.x,vob.position.y,vob.position.z};
  s.sndRadius = vob.radius;
  // NOTE: in original-game zCVobSound::CalcVolumeScale (Gothic2.exe @0x0063dde0) the playback
  // volume is scaled by sndVolume*0.01 (the 0..100 sndVolume property mapped to 0..1); OpenGothic
  // omitted this and played every world VobSound at the raw SoundFX-definition volume.
  s.vol       = vob.volume/100.f;
```

OLD (`WorldSound::tick`, where the instance is created):
```
    i.current = implAddSound(*snd,i.pos,i.sndRadius);
    if(!i.current.isEmpty()) {
      effect.emplace_back(i.current.val);
      i.current.play();
      }
```
NEW:
```
    i.current = implAddSound(*snd,i.pos,i.sndRadius);
    if(!i.current.isEmpty()) {
      i.current.setVolume(i.current.volume()*i.vol); // sndVolume scale, see addSound NOTE
      effect.emplace_back(i.current.val);
      i.current.play();
      }
```

Symbols verified to exist: `WSound::sndRadius`/`pos` (struct at `worldsound.cpp:21`),
`zenkit::VSound::volume` (`Sound.hh:50`), `Sound::volume() const` and `Sound::setVolume(float)`
(`game/world/objects/sound.h:23-24`), `Sound::isEmpty`/`play` used adjacently. `setVolume`
recomputes `occ*vol`, and occlusion is re-applied each frame in `tickSlot`, so multiplying the
stored `vol` is safe.
