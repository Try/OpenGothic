# Land/body-fall impact sound: unsorted material media in `addLandHitEffect`

**Confidence:** Low / DEFERRED (concrete lead, but not promotable to a high-confidence build-verifiable fix; see reason).

## Scope checked
Investigated the body-fall / death-collapse / knockout-landing impact-sound subsystem end to end against the original:

- `oCNpc::onNoHealth` (OpenGothic) vs `oCNpc::DoDie` @0x00736760, `oCNpc::DropUnconscious` @0x00735eb0 — neither original routine emits a dedicated collapse/thud sound; the death/KO collapse sound is **animation-event driven** (`T_DEAD`/`T_DEADB`/unconscious anims carry SFX/SFXGround events, replayed in OpenGothic by `Animation::Sequence::processSfx`). Faithful.
- `oCNpc::OnDamage_Sound` @0x0067a8a0 (DEAD/AARGH voice line) — already matches OpenGothic (`emitSoundSVM`), including the AARGH `_1.._4` variation (already-fixed item, excluded).
- `oCNpc::OnDamage_Anim` @0x00675bd0 — selects `T_DEAD` vs `T_DEADB` by hit angle (<90°) and starts the anim; no separate sound. Faithful.
- `oCNpc::CreateFallDamage` @0x00681da0 — posts an `oCMsgDamage` only; no thud sound. Faithful (fall-XP/vertical-height items already fixed).
- Weapon-hit impact sound: `oCAniCtrl_Human::CreateHit` @0x006b0830 → `zCSoundManager::StartAttackSound` @0x005ed8a0 (does **not** sort its two media) vs OpenGothic `World::addWeaponHitEffect`/`addHitEffect` (unsorted) — consistent.

Conclusion: the core theme is faithfully implemented; the only fresh, concrete asymmetry found is the one below.

## Original fn + address
`zCSoundManager::StartHitSound` @0x005ecae0 (`P:\dev\g2addon\release\ZenGin\_dieter\zSoundMan.cpp`) is the generic "X hits Y" collision/landing impact sound (the routine behind item/body-hits-level "land hit"). At entry it **sorts the two medium-type ids** so the smaller-index medium is always first (`if((int)param_3 < (int)param_2){ swap(param_2,param_3); swap(param_4,param_5); }`) **before** building the impact sound name. This makes the resolved hit-sound instance independent of which side is the "source" vs the "receiver" — the same order-independence the engine applies to the parry sound `zCSoundManager::StartParadeSound` (already mirrored in OpenGothic's `addWeaponBlkEffect`).

## OG file:line
`game/world/world.cpp:769-773` — `World::addLandHitEffect`:
```cpp
Sound World::addLandHitEffect(ItemMaterial src, zenkit::MaterialGroup reciver, const Tempest::Matrix4x4& pos) {
  // IHI - item hits item
  // IHL - Item hits Level
  return addHitEffect(materialTag(src),materialTag(reciver),"IHL",pos);
  }
```
This forms `CS_IHL_<src>_<dst>` with `src,dst` in fixed source-first order (no sort), unlike `StartHitSound`. Caller: `game/physics/dynamicworld.cpp:587` (`setItemHitCallback`, dropped-item/body landing on the level).

## Divergence (candidate)
`StartHitSound` resolves a hit sound that is symmetric in its two media (smaller medium id first); `addLandHitEffect` resolves `CS_IHL_<itemMaterial>_<levelMaterialGroup>` in a fixed direction. Where the loaded `CS_IHL_*` SFX instances are defined for only one ordering of an overlapping material pair (item and level material groups share STONE/WOOD/METAL/…), one of the two matchup directions could resolve to an undefined instance → a silent land/body-fall impact, exactly as the unsorted parry path did before `addWeaponBlkEffect` was fixed.

## Proposed patch — DEFERRED
Not promoted to a fix because three things are unverified and the rules require high confidence:
1. **Sort key mismatch.** `StartHitSound` sorts by **integer medium-type id** (pre-string), whereas OpenGothic's accepted parry fix sorts by **`zSTRING::compare` of the 2-char tag**. For `addWeaponBlkEffect` the two keys coincided closely enough to accept; for `IHL` the operands come from two *different* enums (`ItemMaterial` vs `zenkit::MaterialGroup`) whose tag↔id orderings are not guaranteed to agree, so a naive tag-string sort may not reproduce the engine's medium-id sort.
2. **Instance symmetry unknown.** Whether the shipped `sfx.d` actually defines `CS_IHL_*` for only one ordering (making the bug observable) was not confirmed from the asset scripts.
3. **Scheme-name reconciliation.** The decompiled `StartHitSound` builds its name via `SearchMediumTypeIDList` + an in-place `"…H…"` middle char, which I could not fully reconcile with OpenGothic's explicit `CS_IHL_<a>_<b>` token form; this leaves residual doubt that `StartHitSound` is even the exact original of `addLandHitEffect` rather than a sibling path.

Reason for DEFERRAL: medium-confidence pattern match to an already-accepted fix, but the sort-key and asset-symmetry uncertainties above mean a `std::swap` here could be a no-op or a regression. Needs an `sfx.d` `CS_IHL_*` instance audit before patching. Empty beats a false positive.
