# Auto-equip-best re-fires equip scripts when the best item is already equipped

**Confidence:** Medium-High

## Original function + address

In `Gothic2.exe`, `oCNpc::EquipBestWeapon` (entry `0x0074ef30`) and `oCNpc::EquipBestArmor`
(entry `0x0074f0b0`) both walk the (already display-sorted) inventory category and pick the
**first** item the NPC `CanUse` (plus the melee/ranged category flag, and for ranged a munition
check). Crucially, once that best candidate is selected, **before** calling `EquipWeapon` /
`EquipArmor` each function tests `oCItem::HasFlag(candidate, 0x40000000)` and, if set, **returns
immediately without (re)equipping**. Flag `0x40000000` is the item's "currently equipped" flag
(`oCItem` field `+0x158`, set/cleared by `oCNpc::Equip` `0x00739c90`, `EquipWeapon` `0x0073a030`,
`EquipArmor` `0x0073a490`, `UnequipItem` `0x007326c0`). So when the best usable weapon/armor is the
one already equipped, the original does nothing — no unequip, no re-equip, no script firing. These
are the functions reached on world insert via `oCNpc::Enable` `0x00745d40` (`EquipBestWeapon(2)`
then `EquipBestWeapon(4)`) and via the `EV_EquipBest*` AI actions.

## OpenGothic file:line

`game/game/inventory.cpp`
- `Inventory::equipBestMeleeWeapon` (line 576)
- `Inventory::equipBestRangedWeapon` (line 582)
- `Inventory::equipBestArmor` (line 1032)

Reached from `Inventory::autoEquipWeapons` (line 1013), which `Npc::resetPositionToTA`
(`game/world/objects/npc.cpp:521`) and the `Npc` constructor (`npc.cpp:196`) call on every
NPC on world load / re-insert, and from the `AI_EquipMelee` / `AI_EquipRange` / `AI_EquipBestArmor`
handlers (`npc.cpp:2697-2698`, `2700-2701`, `2694-2695`).

## Divergence

Each `equipBest*` does:

```cpp
auto a = bestMeleeWeapon(owner);   // bestItem() iterates ALL items, including the equipped one
if(a!=nullptr)
  setSlot(melee,a,owner,false);
```

`bestItem` does not skip the already-equipped item, so when the currently-equipped weapon/armor is
still the best, `a` is that same item. `Inventory::setSlot` (line 410) has **no `next==slot`
guard**: it unconditionally runs the unequip branch on the old slot occupant
(`applyArmor(-1)`, `setAsEquipped(false)`, `invokeItem(on_unequip)`) and then the equip branch on
the identical item (`setAsEquipped(true)`, `applyArmor(+1)`, `invokeItem(on_equip)`). The net stat
change is zero, but the item's `on_unequip` and `on_equip` Daedalus callbacks are fired spuriously
every time an NPC is re-inserted (each world load / loading transition runs `resetPositionToTA` for
all NPCs). The original suppresses this via the equipped-flag early-return; OpenGothic does not.

## Proposed patch

Mirror the original's `HasFlag(EQUIPPED) -> return` guard. `Item::isEquipped()` exists
(`game/world/objects/item.h:43`).

`equipBestMeleeWeapon` (OLD):
```cpp
void Inventory::equipBestMeleeWeapon(Npc &owner) {
  auto a = bestMeleeWeapon(owner);
  if(a!=nullptr)
    setSlot(melee,a,owner,false);
  }
```
NEW:
```cpp
void Inventory::equipBestMeleeWeapon(Npc &owner) {
  auto a = bestMeleeWeapon(owner);
  // NOTE: in original-game oCNpc::EquipBestWeapon @0x0074ef30 returns without re-equipping when
  // the best usable item already carries the equipped flag (HasFlag 0x40000000); only swap to a
  // different item, never unequip+re-equip the same one (which would re-fire on_un/on_equip).
  if(a!=nullptr && !a->isEquipped())
    setSlot(melee,a,owner,false);
  }
```

`equipBestRangedWeapon` (OLD):
```cpp
void Inventory::equipBestRangedWeapon(Npc &owner) {
  auto a = bestRangedWeapon(owner);
  if(a!=nullptr)
    setSlot(range,a,owner,false);
  }
```
NEW:
```cpp
void Inventory::equipBestRangedWeapon(Npc &owner) {
  auto a = bestRangedWeapon(owner);
  // NOTE: in original-game oCNpc::EquipBestWeapon @0x0074ef30 returns without re-equipping when
  // the best usable item already carries the equipped flag (HasFlag 0x40000000).
  if(a!=nullptr && !a->isEquipped())
    setSlot(range,a,owner,false);
  }
```

`equipBestArmor` (OLD):
```cpp
void Inventory::equipBestArmor(Npc &owner) {
  auto a = bestArmor(owner);
  if(a!=nullptr)
    setSlot(armor,a,owner,false);
  }
```
NEW:
```cpp
void Inventory::equipBestArmor(Npc &owner) {
  auto a = bestArmor(owner);
  // NOTE: in original-game oCNpc::EquipBestArmor @0x0074f0b0 returns without re-equipping when the
  // best usable armor already carries the equipped flag (HasFlag 0x40000000).
  if(a!=nullptr && !a->isEquipped())
    setSlot(armor,a,owner,false);
  }
```

The guard is correct for every caller: the original's `EquipBestWeapon`/`EquipBestArmor` (reached by
both world-insert auto-equip and the `EV_EquipBest*` AI events) always short-circuits on the
already-equipped best item. When the best item differs from the equipped one, `isEquipped()` is
false and the normal swap proceeds unchanged.
