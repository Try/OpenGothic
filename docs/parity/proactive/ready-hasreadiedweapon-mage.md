# Npc_HasReadiedWeapon ignores readied spell (Mage) state

**Confidence:** High

## Original function + address
`Npc_HasReadiedWeapon` external handler at `FUN_006e6240` (Gothic2.exe). After
resolving the self NPC it calls `oCNpc::GetWeapon` (`0x007377a0`) and returns
`SetReturn(GetWeapon() != null)` — i.e. true whenever the NPC currently has an
item in its active weapon hand.

`oCNpc::GetWeapon` keys off the weapon mode field at `oCNpc+0x250`
(`oCNpc::GetWeaponMode`, `0x00738c40`, valid range 0..7). It returns the in-hand
item only when `1 < mode` (mode >= 2): it handles modes 2/3/4 (1H/2H/Bow) and
mode 6 (**MAG**) by returning the item bound to the right-hand slot string, and
mode 5 (CBow) via the alternate slot string. Modes 0 (NONE) and 1 (FIST) fall
through to `return null`. Crucially, for mode 6 (magic) it returns the readied
rune/scroll item, so `Npc_HasReadiedWeapon` returns **true** for a spellcaster
who has a spell drawn. The companion externals are category-gated and match OG:
`Npc_HasReadiedMeleeWeapon` (`FUN_006e6420`) = `GetWeapon() && HasFlag(2)`
(melee category), `Npc_HasReadiedRangedWeapon` (`FUN_006e6600`) =
`GetWeapon() && HasFlag(4)` (ranged category).

## OpenGothic file:line
`/Users/admin/Downloads/opengothic/game/game/gamescript.cpp:2751` —
`GameScript::npc_hasreadiedweapon`.

## Divergence
OpenGothic returns true only for `W1H || W2H || Bow || CBow`, explicitly omitting
`WeaponState::Mage`. The original returns true for the magic mode too (its
`GetWeapon()` returns the in-hand rune/scroll). Result: an NPC with a spell
readied (`weaponState()==Mage`) reports `Npc_HasReadiedWeapon == FALSE` in
OpenGothic but `TRUE` in Gothic2.exe. OG's own model agrees a weapon is readied
in this state: `Inventory::active` points at the readied spell `numslot`, so
`Npc::activeWeapon()` is non-null in Mage state (this is exactly why the
already-fixed `Npc_GetReadiedWeapon` guards on `NoWeapon/Fist` rather than
returning `activeWeapon()` unconditionally — see gamescript.cpp:2733-2740).
Fist/NoWeapon already agree (both return false), so the Mage state is the sole
mismatch.

## Proposed patch
Grep-verified symbols: `WeaponState::{W1H,W2H,Bow,CBow,Mage}`
(`game/game/constants.h:201-209`), `Npc::weaponState()`.

OLD (`game/game/gamescript.cpp:2751`):
```cpp
bool GameScript::npc_hasreadiedweapon(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  if(npc==nullptr)
    return false;
  auto ws = npc->weaponState();
  return (ws==WeaponState::W1H || ws==WeaponState::W2H ||
          ws==WeaponState::Bow || ws==WeaponState::CBow);
  }
```

NEW:
```cpp
bool GameScript::npc_hasreadiedweapon(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  if(npc==nullptr)
    return false;
  // NOTE: in original-game Npc_HasReadiedWeapon @0x006e6240 -> oCNpc::GetWeapon
  // @0x007377a0 returns the in-hand item for every drawn mode incl. magic
  // (mode 6 returns the readied rune/scroll), so a spellcaster with a spell
  // drawn reports a readied weapon. Mage state must be included here.
  auto ws = npc->weaponState();
  return (ws==WeaponState::W1H || ws==WeaponState::W2H ||
          ws==WeaponState::Bow || ws==WeaponState::CBow ||
          ws==WeaponState::Mage);
  }
```
