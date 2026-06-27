# Npc_GetReadiedWeapon returns a holstered weapon while fist-fighting

**Confidence:** Medium-High

## Original function + address
`Npc_GetReadiedWeapon` external handler `FUN_006eda10` (Gothic2.exe) reads the npc
argument and, when the npc resolves, calls `oCNpc::GetWeapon` (`0x007377a0`) and pushes
its result as the return value (no special "current item" handling).

`oCNpc::GetWeapon` first reads the fight mode at `oCNpc+0x250` and bails out immediately
with `return nullptr` unless the mode is **greater than 1** (`if(1 < fightMode)`). The
ZenGin fight-mode enum is `NONE=0, FIST=1, DAG=2, 1HS=3, 2HS=4, BOW=5, CBOW=6, MAG=7`, so
in **NoWeapon (0)** and **Fist (1)** mode the function returns null; only for an actually
drawn weapon (DAG/1H/2H/Bow/CBow/Mag) does it walk the equipped-node list and return the
in-hand weapon/spell vob.

## OpenGothic file:line
`game/game/gamescript.cpp:2654` — `GameScript::npc_getreadiedweapon`.

## Divergence
OpenGothic returns `npc->activeWeapon()` (the inventory `active` slot) **unconditionally**,
without checking weapon state. During fist combat `Inventory::switchActiveWeaponFist`
points `active` at the holstered melee slot (`active=&melee`; `inventory.cpp:656`), so for a
brawler who has a melee weapon equipped but is fighting with fists, `activeWeapon()` returns
that equipped-but-undrawn weapon. The original returns null in Fist (and NoWeapon) mode.

For all genuinely-drawn states the two agree: melee draws set `active=&melee`, bow/crossbow
set `active=&range`, and `drawSpell -> switchActiveSpell -> switchActiveWeapon(slot>=3)` sets
`active=&numslot[..]` (the rune/spell item) — matching `GetWeapon`'s mage branch. Only the
Fist case (and the trivially-equal NoWeapon case) diverge.

## Proposed patch
Grep-verified symbols: `Npc::weaponState()` (npc.cpp:3900), `WeaponState::NoWeapon` /
`WeaponState::Fist` (constants.h:202-203, already used in this file at lines 2511-2514),
`Npc::activeWeapon()` (npc.cpp:3726).

OLD (`gamescript.cpp:2654`):
```cpp
std::shared_ptr<zenkit::IItem> GameScript::npc_getreadiedweapon(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  if(npc==nullptr) {
    return 0;
    }

  auto ret = npc->activeWeapon();
```
NEW:
```cpp
std::shared_ptr<zenkit::IItem> GameScript::npc_getreadiedweapon(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  if(npc==nullptr) {
    return 0;
    }

  // NOTE: in original-game Npc_GetReadiedWeapon (Gothic2.exe FUN_006eda10) -> oCNpc::GetWeapon
  // (0x007377a0) returns the in-hand weapon only when a weapon is actually drawn (fight mode > FIST):
  // in NoWeapon/Fist mode it returns null. OpenGothic returned the equipped melee weapon even while
  // fist-fighting, because switchActiveWeaponFist points the inventory `active` slot at the holstered
  // melee weapon, so a brawler with a sword equipped wrongly reported a readied weapon.
  auto ws = npc->weaponState();
  if(ws==WeaponState::NoWeapon || ws==WeaponState::Fist)
    return nullptr;

  auto ret = npc->activeWeapon();
```
(The remainder — `makeCurrent(ret); return ret->handlePtr();` else `return nullptr;` — is unchanged.)

## Externals checked
- **Npc_GetReadiedWeapon** (`FUN_006eda10` -> `oCNpc::GetWeapon 0x007377a0`): **divergence above.**
- Mdl_SetModelFatness (`FUN_006fad40`): faithful.
- Mdl_SetModelScale (`FUN_006fabf0`): faithful in practice — OG's `if(npcRef!=nullptr)` guard
  (vs the resolved `npc`) is harmless because `findNpc` only yields null for a null handle; the
  original's extra `AniCtrl::SetVob` rebind after `SetModelScale` is an internal collision refresh
  not modelled by OG's `setScale`. No script-visible difference.
- Mdl_SetVisualBody: structurally faithful.
- Mob_HasItems (`FUN_006f6c70`): NOTE — on a *missing* mob the original returns the item-instance
  argument (a truthy symbol index) rather than 0, because the result reuses the parameter slot; OG
  returns 0. Low confidence / replicating an obscure engine bug with rare reachability — not selected.
- Wld_ExchangeGuildAttitudes (`gamescript.cpp:1732`): already-fixed default verified correct — the
  64x64 (`TAB_ANZAHL`) GIL_ATTITUDES block is overlaid into the 66x66 (`GIL_MAX`) table with stride
  `gilCount`, leaving the ATT_FRIENDLY default on indices 64/65. Overlay loop is intact.
- Wld_AssignRoomToGuild (`gamescript.cpp:1807`): faithful (delegates to `world().assignRoomToGuild`).
- Npc_PlayAni (`FUN_006e52c0`): faithful (`GetModel->StartAni`).
- Npc_PercEnable / Npc_PercDisable (`oCNpc::EnablePerception 0x0075e220` / `DisablePerception
  0x0075e360`): faithful — original keeps a sparse (perc,func) list capped at 33 entries; OG's
  `t>0 && t<PERC_Count` array bounds cover the same valid perception ids.
- Npc_SetRefuseTalk: faithful (`std::max(timeSec*1000,0)` clamp).
- Npc_ExchangeRoutine (`FUN_006ddb50`): faithful (ChangeRoutine + self/other/victim restore).
- Npc_SetActiveSpellInfo (`FUN_006e5880`): faithful.
- Npc_IsWayBlocked (`FUN_006e9da0`): not bound in OG at all — a separate unimplemented-external gap,
  not a wrong-default/sentinel divergence.
