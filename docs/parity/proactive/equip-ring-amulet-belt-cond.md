# Equip: attribute requirement wrongly enforced on rings/amulets/belts

**Confidence:** Medium

## Original function + address

`oCNpc::Equip` (`0x00739c90`) dispatches an item to one of four inventory
categories. Category 1 (weapons) routes to `EquipWeapon` (`0x0073a030`) and
category 2 (armor) routes to `EquipArmor` (`0x0073a490`); both call
`CanUse` (`0x007319b0`), which is the stat-requirement gate (skill index 7 plus
the scripted `cond_atr`/`cond_value` check). Category 4 covers rings
(flag `0x800`), amulets (flag `0x400000`) and belts (flag `0x1000000`). In that
branch the original enforces only *slot-count* limits (at most 1 amulet, 2 rings,
1 belt — by scanning the category for already-equipped items) and then goes
straight to `AddItemEffects` + `SetFlag(0x40000000)`. It never calls `CanUse`
for rings, amulets or belts. So in the original those items have **no**
strength/dexterity/attribute requirement: they always equip.

## OpenGothic location

`game/game/inventory.cpp:402` `Inventory::setSlot`, lines 405-416. Every slot
(including amulet/ringL/ringR/belt, set from `use()` at
`game/game/inventory.cpp:881-892`) runs `checkCondUse` unless `force`. The
inventory UI calls `useItem(clsId, slotHint, /*force=*/false)`
(`game/ui/inventorymenu.cpp:474`), so the check fires for the player.

## Divergence

A ring/amulet/belt whose script defines a non-zero `cond_value` (e.g. a
strength requirement) is blocked in OpenGothic with a "cannot use" message,
whereas the original game equips it unconditionally. Weapons and armor are gated
in both; only ring/amulet/belt differ.

## Proposed patch

```cpp
// game/game/inventory.cpp  — Inventory::setSlot, before the checkCondUse gate
// OLD
  if(next!=nullptr) {
    int32_t atr=0,nValue=0,plMag=0,itMag=0;
    if(!force && !next->checkCondUse(owner,atr,nValue)) {
      vm.printCannotUseError(owner,atr,nValue);
      return false;
      }

    if(!force && !next->checkCondRune(owner,plMag,itMag)) {
      vm.printCannotCastError(owner,plMag,itMag);
      return false;
      }
    }

// NEW
  if(next!=nullptr) {
    // NOTE: in original-game oCNpc::Equip (0x00739c90) only weapons (EquipWeapon)
    // and armor (EquipArmor) call CanUse; rings/amulets/belts equip with no
    // attribute requirement. Skip the cond gate for those slots to match.
    auto nflag = ItmFlags(next->itemFlag());
    bool ringAmuBelt = (nflag & (ITM_RING | ITM_AMULET | ITM_BELT))!=0;
    int32_t atr=0,nValue=0,plMag=0,itMag=0;
    if(!force && !ringAmuBelt && !next->checkCondUse(owner,atr,nValue)) {
      vm.printCannotUseError(owner,atr,nValue);
      return false;
      }

    if(!force && !ringAmuBelt && !next->checkCondRune(owner,plMag,itMag)) {
      vm.printCannotCastError(owner,plMag,itMag);
      return false;
      }
    }
```

`ITM_RING`/`ITM_AMULET`/`ITM_BELT` are item *flags* (`itemFlag()`), defined in
`game/game/constants.h`. Weapon/armor/rune cond checks are unchanged.
