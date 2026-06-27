# Snd_Play3D: static one-shot at snapshot position vs. NPC-attached emitter

**Confidence:** Low (genuine divergence, but marginal audible impact; no surgical fix)

## Original function + address (prose only)
The Daedalus external `Snd_Play3d` is bound (in `DefineExternals_Ulfi` @ `0x006d82e4`)
to the engine routine at `0x006f8840`. That routine pops the sound-name string and the
`C_NPC` instance argument (via the parser's get-instance-and-index helper). If the instance
is null it reports the `"Snd_Play(): illegal params: vob-name: <symbol>"` warning and skips.
Otherwise it asks the global sound system (`zsound`, a `zCSndSys_MSS`) to load the named
sound-FX object (vtable slot +8) and then calls `PlaySound3D(zCSoundFX*, zCVob*, ...)`
(vtable slot +0x30, implementation `zCSndSys_MSS::PlaySound3D` @ `0x004f10f0`). Crucially
the second argument is the **NPC vob pointer itself**, not a frozen coordinate: the engine
parents the active 3D emitter to that vob, so the emitter tracks the NPC's world position
for the whole duration of playback (and is managed/stopped with the vob). The 2D sibling
`Snd_Play` @ `0x006f8790` -> `0x006cbfd0` plays a non-positional 2D sound (vtable +0x28),
which OpenGothic's `Gothic::emitGlobalSound` matches faithfully.

For comparison, the per-SFX variant selection in `zCSndSys_MSS::PlaySound3D` is: index 0 when
fewer than 2 variations, else a `rand()`-chosen variation, else a forced index — which
OpenGothic's `SoundFx::load` (`std::rand()%inst.size()`) reproduces. The SFX volume
(`C_SFX.vol/127`) is likewise applied on both sides. Those parts are NOT divergent.

## OpenGothic file:line
`game/game/gamescript.cpp:3568` (`GameScript::snd_play3d`):

```cpp
auto sfx = ::Sound(*owner.world(),::Sound::T_3D,file,npc->centerPosition(),0.f,false);
sfx.play();
```

`Sound` (`game/world/objects/sound.cpp:10`) stores a fixed `pos` snapshot in
`WorldSound::Effect` and never re-reads the NPC transform. `::Sound::setPosition` exists but
nothing updates a `Snd_Play3D` effect after creation, so the emitter stays at the NPC's
call-time `centerPosition()`.

## Divergence
1. **Position is a static snapshot, not NPC-attached.** The original 3D emitter follows the
   NPC vob as it moves during playback; OpenGothic freezes the emitter at the NPC's position
   at the instant the external ran. For a moving NPC and a non-trivial-length clip the
   panning/attenuation drifts apart.
2. **Emitter anchor height differs.** Original uses the NPC vob's world transform; OpenGothic
   uses `centerPosition()` (bbox center, ~80–90 units above the model origin). At the 3D
   default radius this is inaudible.

Both are real but low-impact: `Snd_Play3D` is overwhelmingly used for short one-shot grunts
where NPC displacement during playback is negligible.

## Proposed patch
**DEFERRED.** A faithful fix requires attaching the active 3D effect to the NPC so its
position is refreshed each tick (the way `WorldSound` updates `worldEff`/vob sounds), plus
choosing the NPC vob's transform origin rather than `centerPosition()`. OpenGothic's `Sound`
returned from `snd_play3d` is a local that goes out of scope immediately (only the
`shared_ptr<WorldSound::Effect>` survives in `effect3d`), so there is no per-NPC handle to
re-position; adding one is structural, not surgical, and the audible benefit is marginal.
No high-confidence one-line change is available, so no edit is proposed.

// NOTE: in original-game Snd_Play3d @0x006f8840 the sound is played via
// zCSndSys_MSS::PlaySound3D(zCSoundFX*, zCVob*, ...) @0x004f10f0 with the NPC vob as the
// 3D source, so the emitter follows the NPC for the duration of playback rather than being
// pinned to a call-time position snapshot.

## Also examined (not divergent / intentional)
- `Snd_Play` 2D path: matches `emitGlobalSound`. The original has **no** `aiProcessPolicy`
  gate, whereas `snd_play`/`snd_play3d` early-return for `aiProcessPolicy>=AiFar2`. This is a
  deliberate, consistently-applied OpenGothic optimization (same guard on `wld_playeffect`)
  and only affects sounds emitted from a very-far NPC's AI tick; treated as intentional, not
  a bug.
- 3D cull radius: OpenGothic defaults `range` to `3500` (`sound.cpp:11`), which matches the
  engine's configurable default 3D radius usage; `C_SFX` (zenkit `ISoundEffect`) exposes no
  per-sound range field, so this is not recoverable as a divergence.
- Variant randomization and SFX volume: faithful on both sides.
