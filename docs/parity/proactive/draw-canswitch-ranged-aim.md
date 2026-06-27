# Draw/Holster: cannot switch weapon while a ranged weapon is readied (aiming)

**Confidence:** High (code-level divergence proven against the decompiler; observable while aiming a bow/crossbow).

## Original function + address
`oCNpc::CanDrawWeapon` (Gothic2.exe `0x006805c0`) is the gate every weapon-draw path
consults. In prose, the original returns "can draw" (1) when ANY of the following holds:
- the anictrl reports `IsStanding`, OR
- the anictrl reports `IsWalking`, OR
- `GetInteractMob() != NULL` (busy at a mobsi), OR
- `GetWeaponMode() == 5` (bow), OR
- `GetWeaponMode() == 6` (crossbow).

Otherwise it returns 0.

The two ranged-mode clauses are not redundant: `oCAniCtrl_Human::IsStanding`
(`0x006adee0`) returns true only when the *active* animation is the per-weapon-mode
standing-idle (`s_<mode>`); while a bow/crossbow is being aimed the active ani is the aim
sequence, so both `IsStanding` and `IsWalking` are false. The `GetWeaponMode()==5/6`
clauses exist precisely so that a readied ranged weapon can always be switched/holstered,
including from the aim stance. (Weapon-mode numbering is confirmed by
`oCNpc::SetWeaponMode` `0x00739940` and `oCNpc::GetNextWeaponMode` `0x00739a30`:
5 = bow flag `0x80000`, 6 = crossbow flag `0x100000` — the same 5/6 OpenGothic writes into
`hnpc->weapon` in `drawWeaponBow`.)

## OpenGothic file:line
`game/world/objects/npc.cpp:3815` — `Npc::canSwitchWeapon()` (the OG mirror of
`oCNpc::CanDrawWeapon`, already cited in-file for the mobsi case).

## Divergence
OG's allow-list is `BS_STAND | BS_WALK | BS_RUN | BS_SNEAK | BS_NONE` plus the
`interactive()` (mobsi) exception. It has no equivalent of the `GetWeaponMode()==5/6`
clause. While aiming a bow or crossbow the body state is `BS_AIMNEAR` (24) / `BS_AIMFAR`
(25) (set in `mdlvisual.cpp:735`; consumed at `npc.cpp:4262/4278`), neither of which is in
the allow-list. Therefore in OpenGothic `canSwitchWeapon()` returns false during aim, and
`drawWeaponMelee`/`drawWeaponFist`/`drawWeaponBow`/`drawMage` (all guarded by
`canSwitchWeapon()`, npc.cpp:3856/3880/3906/3929) silently no-op. The AI message paths
`AI_DrawWeapon*`/`AI_DrawSpell` (npc.cpp:2807-2831) are gated the same way and would stall.
In the original, with weapon mode 5/6 the switch is always permitted. (Holstering via
`closeWeapon` is unaffected — it does not consult `canSwitchWeapon`.)

## Proposed patch
Add the ranged-mode exception, mirroring the original's `GetWeaponMode()==5/6` clauses.
All symbols grep-verified: `weaponState()` is a `const` accessor returning `WeaponState`
(npc.h:264); `WeaponState::Bow`/`WeaponState::CBow` exist (constants.h:206-207).

OLD (npc.cpp:3821-3826):
```cpp
  if(interactive()!=nullptr)
    return true;
  auto bs = bodyStateMasked();
  if(bs==BS_STAND || bs==BS_WALK || bs==BS_RUN || bs==BS_SNEAK || bs==BS_NONE)
    return true;
  return false;
```

NEW:
```cpp
  if(interactive()!=nullptr)
    return true;
  // NOTE: in original-game oCNpc::CanDrawWeapon (Gothic2.exe 0x006805c0) also returns true when
  // GetWeaponMode()==5 (bow) / ==6 (crossbow): a readied ranged weapon can always be switched,
  // even from non-stand/walk states. IsStanding/IsWalking are false during the aim ani
  // (BS_AIMNEAR/BS_AIMFAR), so without this clause OG could not switch off a drawn bow while aiming.
  auto ws = weaponState();
  if(ws==WeaponState::Bow || ws==WeaponState::CBow)
    return true;
  auto bs = bodyStateMasked();
  if(bs==BS_STAND || bs==BS_WALK || bs==BS_RUN || bs==BS_SNEAK || bs==BS_NONE)
    return true;
  return false;
```
