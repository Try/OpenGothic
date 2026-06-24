# Parry/block (IAI) effect-name ordering is not canonicalized

**Confidence:** Low–Medium (real ordering divergence; **DEFERRED** because the fix's necessity rests on an architectural inference, see below)

## Original function + address (prose only)

The vanilla parry/block clang+spark effect is produced by
`oCAniCtrl_Human::StartParadeEffects` (Gothic2.exe @0x006b16f0). It does the
following:

- It fetches a *weapon-type* tag for **both** participants — the defending NPC
  and the attacking NPC — through the virtual `GetWeaponHitString`
  (`oCAniCtrl_Human::GetWeaponHitString` @0x006aeef0). That helper returns
  weapon-*type* strings, not material strings: case 1 → `"FIST"`, case 2 →
  `"DAG"`, case 3 → `"1HS"`, and so on (default `"FIST"`).
- It runs the effect path only when **neither** participant is in fist weapon
  mode (`GetWeaponMode(self)!=1 && GetWeaponMode(attacker)!=1`).
- It then **lexicographically sorts the two tag strings** (`basic_string::compare`,
  then an `if(cmp<0) … else …` that concatenates them in opposite order) and
  builds the particle name `CPFX_IAI_<smaller>_<larger>`. The result is therefore
  **order-independent**: a 1HS weapon parrying a dagger and a dagger parrying a
  1HS weapon yield the same `CPFX_IAI_…` name.
- The audible clang itself is emitted separately by
  `zCSoundManager::StartAttackSound` (@0x005ed8a0), whose internal medium ids are
  also canonicalized (the sibling `StartHitSound` @0x005ecae0 explicitly swaps so
  the lower medium id is always first before composing the name).

## OpenGothic file:line

- `game/graphics/mdlvisual.cpp:406` `MdlVisual::emitBlockEffect` — the block path,
  reached from `game/world/objects/npc.cpp:2073`.
- `game/world/world.cpp:767` `World::addWeaponBlkEffect` → `World::addHitEffect`
  (`game/world/world.cpp:772`), which builds `CS_IAI_<src>_<dst>` /
  `CPFX_IAI_<src>_<dst>` in **fixed (attacker-weapon, defender-weapon) order**, no
  sort.

## Divergence

Within OpenGothic's own naming scheme, the IAI block effect name is built in a
**fixed src→dst order**, whereas the original engine canonicalizes the two tags
(sorts them) before composing the name. If only one of the two orderings of a
material pair has a backing asset, OpenGothic will pick the wrong/missing variant
for half of the attacker/defender combinations (e.g. attacker=WOOD, defender=METAL
→ OG `CS_IAI_WO_ME`, but the canonical name is `*_ME_WO`).

## Proposed patch — DEFERRED

Reason: two unknowns block a high-confidence, build-verifiable fix.

1. **Tag basis mismatch.** Vanilla's `StartParadeEffects` sorts *weapon-type*
   tags (`1HS`/`DAG`/`FIST`/…) and emits `CPFX_IAI_…`, while OpenGothic's
   `addWeaponBlkEffect` sorts nothing and uses *material* tags (`ME`/`WO`/… from
   `materialTag(ItemMaterial)`, enum `MAT_WOOD=0 … MAT_GLAS=5` in
   `game/game/constants.h:252`). OpenGothic's `CS_IAI_<material>_<material>` is a
   different naming family than vanilla's `CPFX_IAI_<weapontype>_<weapontype>`;
   asserting that the canonical-ordering intent carries over to the material-tag
   names is an inference, not a verified 1:1 fact.
2. **Asset existence is unverifiable from the exe.** Whether the original ships
   one or both orderings of each `IAI` pair is a packed-VDFS WAV/PFX question; the
   `CS_IAM_*` / `CS_MAM_*` / `CS_IAI_*` names are loaded by direct resource name
   and are **not** defined as Daedalus SFX instances (verified against
   `_work/Data/Scripts/system/SFX/SfxInst.d`), so the set of available variants
   cannot be enumerated without unpacking the audio volumes.

The low-risk candidate fix (if the above are later resolved in OG's favour) is to
sort the two tags in `World::addHitEffect` / `addWeaponBlkEffect` so `src<=dst`
before composing `string_frm("CS_",scheme,'_',src,'_',dst)`, matching the
original's canonicalization. Grep-verified symbols for that change:
`World::addHitEffect` (`game/world/world.cpp:772`), the `string_frm` composition
(`game/world/world.cpp:776`,`:779`), and `materialTag(ItemMaterial)`
(`game/world/world.cpp:25`).

```
// NOTE: in original-game oCAniCtrl_Human::StartParadeEffects @0x006b16f0 the two
// participant tags are lexicographically sorted before building CPFX_IAI_<a>_<b>,
// and zCSoundManager::StartHitSound @0x005ecae0 swaps so the lower medium is first,
// so the parry/collision effect name is order-independent.
```
