# Melee draw/sheathe SFX bypass the C_SFX instance system (wrong sample + full volume; phantom sheathe sound)

**Confidence:** High

## Original fn + address

`oCNpc::DoDoAniEvents` @ `0x00742a20` processes the per-frame model anim eventTags.
The `DEF_DRAWSOUND` handler reads the in-hand weapon's material (`oCItem` GetMaterial
vfunc, slot `+0x98`) and builds an **SFX instance name** `"DrawSound" + "_" + <ME|WO>`
(`"Drawsound_ME"` for `MAT_METAL`, `"Drawsound_WO"` for `MAT_WOOD`; any other material
hits the `"DEF_DRAWSOUND : No soundmaterial found"` path and stays silent). The
`DEF_UNDRAWSOUND` handler builds `"UnDrawSound_ME"` / `"UnDrawSound_WO"` the same way.
Each name is handed to `zCSndSys_MSS::LoadSoundFXScript` @ `0x004ee120`, which looks the
name up as a **Daedalus C_SFX symbol**; if the symbol does not exist it logs an error and
returns `NULL`, and the caller's `if(sfx != null)` guard then plays nothing — there is **no
filename fallback** (it never loads a bare `*.WAV`).

The shipped data (`_work/Data/Scripts/system/SFX/SfxInst.d`) defines:
- `INSTANCE Drawsound_ME    (C_SFX_DEF) { file = "Sword_Draw_01.wav"; vol = 25; }`
- `INSTANCE Drawsound_ME_A1 (C_SFX_DEF) { file = "Sword_Draw_01.wav"; vol = 25; }`  (the `_A`/variant entry)
- `INSTANCE Drawsound_WO    (C_SFX_DEF) { file = "Sword_Draw_01.wav"; vol = 25; }`
- `INSTANCE Drawsound_Bow   (C_SFX_DEF) { file = "Woosh_After_01.wav"; vol = 60; }`

There is **no `UnDrawSound_*` instance anywhere** in the system or addon scripts.

Consequences in the original: drawing a melee weapon plays `Sword_Draw_01.wav` at
**vol = 25** (~20%); **sheathing plays nothing** (the `UnDrawSound_*` symbols don't exist →
`LoadSoundFXScript` returns null → silent). The matching `HumanS.mds` confirms the timing:
`DEF_DRAWSOUND`/`DEF_UNDRAWSOUND` sit on the same frame as `DEF_FIGHTMODE` in every melee
transition, so OpenGothic's coupling of the sound to the fight-mode (`SET_FIGHT_MODE`) event
is timing-faithful — only the sound resolution is wrong.

## OG file:line

`game/world/objects/npc.cpp:1991-2001` (`Npc::implSetFightMode`).

```cpp
sfxWeapon = ::Sound(owner,::Sound::T_Regular,"UNDRAWSOUND_ME.WAV",at,2500,false); else
sfxWeapon = ::Sound(owner,::Sound::T_Regular,"UNDRAWSOUND_WO.WAV",at,2500,false);
...
sfxWeapon = ::Sound(owner,::Sound::T_Regular,"DRAWSOUND_ME.WAV",at,2500,false); else
sfxWeapon = ::Sound(owner,::Sound::T_Regular,"DRAWSOUND_WO.WAV",at,2500,false);
```

## Divergence

The `.WAV` suffix forces `Sound::Sound` (`game/world/objects/sound.cpp:27-29`) down the
`loadSoundWavFx` raw-file path instead of `loadSoundFx` (the C_SFX-instance path). The raw
path constructs `SoundFx(Tempest::Sound&&)` with **vol = 127 (full)** and no variants, and
plays a *different file altogether* (`DRAWSOUND_ME.WAV` is an orphaned sample in `Sounds.vdf`
that no vanilla C_SFX references). Net result vs original:

- **Draw:** OG plays `DRAWSOUND_ME.WAV`/`DRAWSOUND_WO.WAV` at **full volume**; original plays
  `Sword_Draw_01.wav` at **vol 25**. Wrong sample, ~5× too loud.
- **Sheathe:** OG plays an audible `UNDRAWSOUND_*.WAV` at full volume; original plays **nothing**
  (no `UnDrawSound_*` instance).

This also makes melee the lone outlier: the bow/crossbow draw already goes through the
instance system (`Drawsound_Bow`, vol 60) via the data-driven `*eventSFX` path
(`processSfx → emitSoundEffect → loadSoundFx`), exactly as the original does.

## Proposed patch

Drop the `.WAV` extension so the four names resolve as C_SFX instances (case-insensitive;
`SoundFx` already retries upper-cased). Draw then plays `Sword_Draw_01.wav` @ vol 25 (with the
`_A1` variant); sheathe finds no instance → `SoundFx` stays empty → silent — both matching the
original. Range stays 2500 (OG's existing listener-range choice, unchanged).

```cpp
// OLD
      if(melee->handle().material==ItemMaterial::MAT_METAL)
        sfxWeapon = ::Sound(owner,::Sound::T_Regular,"UNDRAWSOUND_ME.WAV",at,2500,false); else
        sfxWeapon = ::Sound(owner,::Sound::T_Regular,"UNDRAWSOUND_WO.WAV",at,2500,false);
...
      if(melee->handle().material==ItemMaterial::MAT_METAL)
        sfxWeapon = ::Sound(owner,::Sound::T_Regular,"DRAWSOUND_ME.WAV",at,2500,false); else
        sfxWeapon = ::Sound(owner,::Sound::T_Regular,"DRAWSOUND_WO.WAV",at,2500,false);

// NEW
      // NOTE: in original-game oCNpc::DoDoAniEvents @0x00742a20 the DEF_DRAWSOUND/DEF_UNDRAWSOUND
      // eventTags resolve a Daedalus C_SFX *instance* (Drawsound_ME/Drawsound_WO, file
      // "Sword_Draw_01.wav", vol=25) via zCSndSys_MSS::LoadSoundFXScript @0x004ee120 -- which
      // returns null for an undefined symbol with no .WAV fallback. There is no UnDrawSound_*
      // instance, so the original is silent on sheathe. Route through the C_SFX-instance loader
      // (no ".WAV") to get the correct sample/volume on draw and silence on sheathe, matching the
      // already-correct data-driven bow path (Drawsound_Bow, vol=60).
      if(melee->handle().material==ItemMaterial::MAT_METAL)
        sfxWeapon = ::Sound(owner,::Sound::T_Regular,"UNDRAWSOUND_ME",at,2500,false); else
        sfxWeapon = ::Sound(owner,::Sound::T_Regular,"UNDRAWSOUND_WO",at,2500,false);
...
      if(melee->handle().material==ItemMaterial::MAT_METAL)
        sfxWeapon = ::Sound(owner,::Sound::T_Regular,"DRAWSOUND_ME",at,2500,false); else
        sfxWeapon = ::Sound(owner,::Sound::T_Regular,"DRAWSOUND_WO",at,2500,false);
```

(Optional, lower priority: the original also stays silent for melee weapons whose material is
neither `MAT_WOOD` nor `MAT_METAL` — the `"No soundmaterial found"` branch — but no vanilla
melee weapon uses another material, so that gate is not part of this fix.)
