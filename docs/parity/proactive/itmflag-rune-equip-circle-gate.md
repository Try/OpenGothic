# Rune (ITM_CAT_RUNE) equip wrongly blocked by magic-circle / attribute gate

**Confidence:** High

## Original function + address
`oCNpc::Equip` (Gothic2.exe `0x00739c90`) dispatches an item by
`oCNpcInventory::GetCategory` (`0x0070c690`), which maps the item's `main_flag`
(field at +0x154) to a category number. `ITM_CAT_RUNE` (`0x200`, i.e. `1<<9`)
maps to **category 3**.

For category 3, `Equip` only registers/deregisters the rune in the magic book
(`oCMag_Book::Register`/`DeRegister`, capped at 7 spells) by toggling the item's
active flag `0x40000000`. It returns **without ever calling `oCNpc::CanUse`**.

`oCNpc::CanUse` (`0x007319b0`) is precisely the gate that compares the player's
mage talent `GetTalentSkill(this, 7)` (TALENT_MAGE = magic circle) against the
item's required circle (`mag_circle`, field at +0x270) and fires `G_CANNOTCAST`
on failure. Its xrefs confirm it is invoked from `EquipWeapon`, `EquipArmor`,
`EquipFarWeapon`, `UseItem`, and the ranged-weapon branch of `Equip` — but **not**
from the category-3 rune branch. So the original imposes no circle requirement
(and no `cond_atr` requirement) at the moment a rune is placed in the spell belt;
underpowered casting is left to the spell script / mana logic at cast time.

## OpenGothic file:line
`game/game/inventory.cpp:410-429` (`Inventory::setSlot`), reached for runes via
`use()` -> `equipNumSlot()` (`inventory.cpp:900-906`, `852-864`).

## Divergence
`setSlot` skips the cond gate only for ring/amulet/belt (`ringAmuBelt`). Runes
are none of those, so for a rune it runs both `checkCondUse` (cond_atr/cond_value)
and `checkCondRune` (`mageCycle() >= mag_circle`). `checkCondRune` is effectively
a rune-only gate — for every non-rune item `mag_circle==0`, so `cPl>=0` always
passes; it can only ever block a rune. Because the original registers runes with
no `CanUse` call, OpenGothic wrongly refuses to put a rune into the spell belt
(printing the cannot-cast / cannot-use error) whenever the player's magic circle
is below the rune's circle, or a rune attribute requirement is unmet — e.g. a
higher-circle rune found, bought, or stolen before the circle is learned. In the
original it is registered fine and merely cannot be cast for lack of mana.

`mageCycle()` = `talentSkill(TALENT_MAGE)` (npc.cpp:1226), and TALENT_MAGE = 7,
matching `CanUse`'s `GetTalentSkill(...,7)` exactly.

## Proposed patch
Extend the existing cond-gate skip to also cover runes, mirroring `Equip`'s
category-3 path that registers a rune without `CanUse`.

OLD (`game/game/inventory.cpp`, in `Inventory::setSlot`):
```cpp
    const bool ringAmuBelt = (uint32_t(next->itemFlag()) & (ITM_RING|ITM_AMULET|ITM_BELT))!=0;
    int32_t atr=0,nValue=0,plMag=0,itMag=0;
    if(!force && !ringAmuBelt && !next->checkCondUse(owner,atr,nValue)) {
      vm.printCannotUseError(owner,atr,nValue);
      return false;
      }

    if(!force && !ringAmuBelt && !next->checkCondRune(owner,plMag,itMag)) {
      vm.printCannotCastError(owner,plMag,itMag);
      return false;
      }
```

NEW:
```cpp
    const bool ringAmuBelt = (uint32_t(next->itemFlag()) & (ITM_RING|ITM_AMULET|ITM_BELT))!=0;
    // NOTE: in original-game oCNpc::Equip (Gothic2.exe 0x00739c90) a rune (main_flag
    // ITM_CAT_RUNE -> oCNpcInventory::GetCategory @0x0070c690 category 3) is registered
    // in the magic book without ever calling CanUse (0x007319b0); only weapons/armor and
    // the ranged-weapon branch invoke it. checkCondRune (mageCycle>=mag_circle) is a
    // rune-only gate, so applying it here blocked the player from belting a higher-circle
    // (or attribute-gated) rune that the original lets you register and select.
    const bool noCondGate  = ringAmuBelt || next->isSpellOrRune();
    int32_t atr=0,nValue=0,plMag=0,itMag=0;
    if(!force && !noCondGate && !next->checkCondUse(owner,atr,nValue)) {
      vm.printCannotUseError(owner,atr,nValue);
      return false;
      }

    if(!force && !noCondGate && !next->checkCondRune(owner,plMag,itMag)) {
      vm.printCannotCastError(owner,plMag,itMag);
      return false;
      }
```

Grep-verified symbols: `Item::isSpellOrRune()` (item.h:70, item.cpp:272 = `mainFlag() & ITM_CAT_RUNE`),
`ITM_CAT_RUNE` (constants.h:332), `Item::itemFlag()`/`mainFlag()`, `checkCondUse`/`checkCondRune`
(item.cpp:358/370), `printCannotCastError`/`printCannotUseError` (gamescript). Casting remains
script/mana-gated (invokeSpell, npc.cpp:3247), so removing the equip-time circle block does not
let an underpowered spell actually fire.
