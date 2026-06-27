# Bow/crossbow draw plays the draw-sound twice (hardcoded + data-driven eventSFX)

**Confidence:** High

## Original function + address
`oCNpc` ani-event dispatcher `DoDoAniEvents` @ `0x00742a20` (Gothic2.exe).
Two distinct, non-overlapping event paths produce weapon-draw sound:

- **Melee (`DEF_DRAWSOUND` / `DEF_UNDRAWSOUND`)** is engine-hardcoded inside
  `DoDoAniEvents`: it looks up the weapon in the RIGHTHAND/SWORD/LONGSWORD/LEFTHAND
  slots, reads the weapon material via the item vtable (+0x98), and plays
  `DrawSound`/`UnDrawSound` for material WOOD(0) or METAL(2) only (the
  `DEF_DRAWSOUND : No soundmaterial found.` branch is otherwise silent). The
  `DEF_FIGHTMODE` branch of the same function only calls `SetWeaponMode2` and plays
  **no** sound.
- **Bow/crossbow** has **no** `DEF_DRAWSOUND` at all. Instead the draw/holster
  transition anims carry an ordinary `*eventSFX (7 "Drawsound_Bow")` data tag, which
  the generic SFX-event path plays. Verified in the shipped overlays
  `Humans_BowT1/BowT2/CBowT1/CBowT2.mds` (e.g. `t_Bow_2_BowRun`,
  `t_CBow_2_CBowRun`): every bow/crossbow draw **and** holster anim has
  `DEF_FIGHTMODE` + `eventSFX "Drawsound_Bow"` on the same frame, and the
  `DEF_FIGHTMODE` event plays nothing on its own.

So in the original the bow/crossbow draw sound comes from **exactly one** source: the
data-driven `eventSFX "Drawsound_Bow"`.

## OpenGothic file:line
`game/world/objects/npc.cpp:1994-1998` — `Npc::implSetFightMode`.

## Divergence
OpenGothic plays the bow/crossbow draw sound from **two** sources at once:

1. The MDS `*eventSFX "Drawsound_Bow"` fires through the normal SFX path
   (`Animation::Sequence::processSfx` -> `Npc::emitSoundEffect`,
   `npc.cpp:3280`), which uses `::Sound(owner,T_Regular,"Drawsound_Bow",...)`.
2. `implSetFightMode` *additionally* hardcodes
   `::Sound(owner,T_Regular,"DRAWSOUND_BOW",...)` whenever `ev.weaponCh` is
   `BOW`/`CROSSBOW`.

Both calls resolve the same SFX instance the same way (`emitSoundEffect` and the
hardcoded line use the identical `::Sound(...,T_Regular,...)` constructor) and both
fire on the same event frame (frame 7), so the bow/crossbow draw "shing" plays
twice (audible doubling/phasing). Melee is **not** affected: melee draw anims have
no `eventSFX`, so OpenGothic's hardcoded melee `DRAWSOUND_ME/WO.WAV` correctly
replicates the engine's `DEF_DRAWSOUND` and must stay. The bow branch is the only
duplicate. (Holster is already correct in OpenGothic: the `weaponCh==NONE` undraw
branch is guarded on `ws==W1H||W2H`, so it doesn't re-fire for bows, and the
`eventSFX` on `t_BowRun_2_Bow` carries the holster sound by itself.)

## Proposed patch
Remove the hardcoded bow/crossbow draw-sound branch; the data-driven `eventSFX`
already provides it, matching the original.

OLD (`game/world/objects/npc.cpp`, in `Npc::implSetFightMode`):
```cpp
  else if(ev.weaponCh==zenkit::MdsFightMode::BOW || ev.weaponCh==zenkit::MdsFightMode::CROSSBOW) {
    auto at = centerPosition();
    sfxWeapon = ::Sound(owner,::Sound::T_Regular,"DRAWSOUND_BOW",at,2500,false);
    sfxWeapon.play();
    }
```

NEW:
```cpp
  // NOTE: in original-game DoDoAniEvents @0x00742a20 the bow/crossbow draw sound is
  // NOT engine-hardcoded like the melee DEF_DRAWSOUND path -- the bow/crossbow draw &
  // holster transition anims (Humans_BowT*/CBowT*.mds) instead carry an ordinary
  // *eventSFX "Drawsound_Bow" that the generic SFX path already plays. Playing an
  // extra hardcoded "DRAWSOUND_BOW" here doubled the sound on every bow/crossbow draw.
```
(i.e. delete the `else if` branch entirely; melee draw/undraw branches above are kept
because melee draw anims have no `eventSFX` and rely on the engine's DEF_DRAWSOUND.)
