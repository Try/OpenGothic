# Water-entry splash sound played as raw `.WAV` (`CS_INTRO_WATERSPLASH.WAV`) instead of a C_SFX instance

**Confidence:** Low (DEFERRED) — structural divergence is real, but the *original-engine* basis cannot be confirmed.

## Original fn + address (prose)
The NPC water-entry "splash" is, in OpenGothic, an engine-side reconstruction in `MoveAlgo`
(`emitWaterSplash`), called when an NPC transitions into the `Swim` state with gravity/fall-speed
(a plunge into water). It spawns the `PFX_WATERSPLASH` particle emitter and then plays a discrete
splash sound.

I could not locate a corresponding *engine* sound emission in `Gothic2.exe`:
- The only engine code that spawns a water-splash particle effect is the weather/sky droplet
  spawner `FUN_0052a9f0` (`zBsp.cpp`, references `PFX_WATERSPLASH_SEA` / `PFX_WATERLIGHT`); it is
  **sound-less** and unrelated to NPC entry.
- `oCAniCtrl_Human::CheckWaterLevel` / `::IsInWater` / `::HackNPCToSwim` contain **no** sound or
  splash-FX call.
- A full `wde strings` sweep finds **no** reference anywhere in the exe to the string
  `CS_INTRO_WATERSPLASH`, nor to a file `CS_INTRO_WATERSPLASH.WAV` (only `PFX_WATERSPLASH`,
  `PFX_WATERSPLASH_SEA` as particle names). The `CS_INTRO_` prefix indicates a **cutscene/intro**
  C_SFX instance referenced from Daedalus/`.CSL` script, not engine code.

Consequence: there is no evidence the original plays *any* sound on gameplay water-entry; the OG
`emitWaterSplash` sound appears to be an OpenGothic addition that borrows the existing intro splash
sample. So the usual "original resolves a C_SFX instance, OG hardcodes a raw WAV" parity claim is
**not verifiable here**.

## OG file:line
`game/game/movealgo.cpp:698`
```cpp
npc.emitSoundEffect("CS_INTRO_WATERSPLASH.WAV",2500,false);
```

## Divergence (the structural issue, independent of parity)
The literal carries a `".WAV"` suffix, so `Sound::Sound` (`game/world/objects/sound.cpp:27`)
routes it through `Gothic::loadSoundWavFx` — the raw-file loader. `SoundFx(Tempest::Sound&&)`
(`game/sound/soundfx.cpp:44-47`) stores the sample at vol `127/127 = 1.0` (full volume) with no
instance volume/range. If `CS_INTRO_WATERSPLASH` is defined as a C_SFX instance (likely, given the
intro cutscene uses it), its scripted `vol` (and any variants) are ignored — identical sample, but
played louder/flatter than the instance would. This is the same class of bug as the already-fixed
melee `DRAWSOUND_*.WAV`, but **weaker**: because the instance's `file` equals the instance name +
`.wav`, the raw path still plays the correct *sample* (only volume/range differ), whereas the melee
case played a wrong orphan sample.

## Proposed patch
**DEFERRED.** Reasons:
1. No original-engine code path plays a water-entry splash sound (the engine splash spawner is
   sound-less; the sound has no string reference in the exe), so there is no confirmed original
   "C_SFX instance" behavior to match — the whole `emitWaterSplash` sound looks OG-invented.
2. The fix would be to drop `".WAV"` so it resolves via `loadSoundFx` (instance vol/range). But
   `CS_INTRO_WATERSPLASH` is a Daedalus instance living in `Sfx.dat`, which cannot be grep-verified
   from `Gothic2.exe`. If the instance is absent at this call site's content, dropping `".WAV"`
   yields **silence** (the `.WAV` fallback at `soundfx.cpp:33` keys on the suffix and would no
   longer fire), a regression versus today's audible-but-loud behavior.

Candidate (apply only after confirming `CS_INTRO_WATERSPLASH` is a C_SFX instance in the target
`Sfx.dat`):
```cpp
// OLD
npc.emitSoundEffect("CS_INTRO_WATERSPLASH.WAV",2500,false);
// NEW
// NOTE: in original-game no engine swim-entry splash sound was found (zBsp.cpp FUN_0052a9f0
// spawns PFX_WATERSPLASH_SEA droplets silently; CS_INTRO_WATERSPLASH has no string ref in the exe,
// implying a Daedalus/cutscene C_SFX instance). Dropping ".WAV" routes through loadSoundFx so the
// instance's vol/range apply instead of raw vol=127, mirroring the DRAWSOUND_* fix — but only if
// the instance exists in Sfx.dat, else this goes silent.
npc.emitSoundEffect("CS_INTRO_WATERSPLASH",2500,false);
```

## Sweep result (other candidates)
`CS_INTRO_WATERSPLASH.WAV` is the **only** remaining engine-hardcoded raw-`.WAV` / non-instance
sound literal in the NPC/MoveAlgo/Interactive/Item sound paths (the melee draw/sheathe and bow
cases are already fixed). Body-fall/landing thud, footstep, hit-impact, parry/block clang, item
pickup/drop, KO collapse, jump-land are all driven by animation `eventTag` SFX names
(`animation.cpp` `processSfx` -> `emitSoundEffect`/`emitSoundGround`), i.e. they already pass C_SFX
**instance** names (no suffix) through `loadSoundFx`. No divergence found there.
