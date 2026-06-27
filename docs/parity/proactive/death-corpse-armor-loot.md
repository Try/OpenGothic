# Armour is lootable from corpses/unconscious bodies (should be hidden)

**Confidence:** High

## Original function + address

`oCNpcContainer::CreateList` (Gothic2.exe @ 0x0070b570) builds the item list shown
when the player loots a dead or unconscious NPC (entered via `oCNpc::OpenDeadNpc`
@ 0x00762970, which wraps the down NPC in an `oCNpcContainer`). For every item in
the NPC's inventory, CreateList inserts it into the lootable list **only when**
`oCItem::HasFlag(item, 0x10) == 0` **AND** `oCItem::HasFlag(item, 0x40000000) == 0`.

`oCItem::HasFlag` (@ 0x007126d0) tests the merged item-flag field at `oCItem+0x158`.
`oCItem::InitByScript` (@ 0x00711bd0) OR's the script `mainflag` member (oCItem+0x154)
into that field, so the merged field carries both the weapon-category `flags` bits
(verified by `oCNpc::EquipItem` @ 0x007323c0 testing 0x2000=ITEM_DAG, 0x4000=ITEM_SWD,
0x8000=ITEM_AXE, 0x10000=ITEM_2HD_SWD ... at +0x158) and the `mainflag` category bits.
Therefore:
- `0x40000000` is the runtime **equipped** flag (set by `oCNpc::EquipItem`, cleared on
  unequip / `DoDropVob`).
- `0x10` is the **ITEM_KAT_ARMOR** mainflag bit (1<<4).

Net effect in the original: armour items are **never** shown in a corpse/unconscious
loot list, whether equipped or merely carried; equipped items of any kind are likewise
hidden. (If the resulting list is empty, OpenDeadNpc plays a manipulate "nothing"
gesture instead of opening the inventory window.)

## OpenGothic file:line

`game/game/inventory.cpp:76-80` — `Inventory::Iterator::skipHidden()`, `T_Ransack` branch:

```cpp
if(type==T_Ransack) {
  while(at<it.size() && it[at]->isEquipped()) {
    ++at;
    }
  }
```

The `T_Ransack` iterator drives all three loot paths: the `RansackPage`
(`game/ui/inventorymenu.cpp:72`), the `ransack()` validity / "nothing to get" check
(`inventorymenu.cpp:150`), and the dead-body focus highlight
(`game/world/worldobjects.cpp:779`).

## Divergence

OpenGothic's `T_Ransack` filter skips only **equipped** items. It does not skip
**armour**. The original additionally hides every ITEM_KAT_ARMOR item unconditionally.
Consequence: a downed (dead or unconscious) NPC that carries an **unequipped** armour
in its inventory exposes that armour as lootable in OpenGothic, and the corpse is shown
as a loot-focus target, whereas in the original the armour is filtered out (and a
corpse holding only armour shows no loot at all). Most NPCs only have their worn
(equipped) armour, which both engines already hide via the equipped check, so the visible
break is limited to NPCs/monsters carrying spare armour as inventory loot.

Grep-verified OpenGothic symbols:
- `Item::isArmor()` — `game/world/objects/item.cpp:301` (`mainFlag() & ITM_CAT_ARMOR`).
- `Item::mainFlag()` — `item.cpp:243` (reads `hitem->main_flag`).
- `ITM_CAT_ARMOR = 1 << 4` (== 0x10) — `game/game/constants.h:327`.
- `Item::isEquipped()` — `game/world/objects/item.h:43`.

## Proposed patch

`game/game/inventory.cpp`, `Inventory::Iterator::skipHidden()`:

OLD:
```cpp
  if(type==T_Ransack) {
    while(at<it.size() && it[at]->isEquipped()) {
      ++at;
      }
    }
```

NEW:
```cpp
  if(type==T_Ransack) {
    // NOTE: in original-game oCNpcContainer::CreateList (Gothic2.exe 0x0070b570) the
    // corpse/unconscious loot list inserts an item only when HasFlag(0x10)==0 AND
    // HasFlag(0x40000000)==0. In the merged item-flag field (oCItem+0x158, with mainflag
    // OR'd into flags by oCItem::InitByScript @0x00711bd0) 0x10 == ITM_CAT_ARMOR
    // (mainflag 1<<4) and 0x40000000 == the equipped flag (set by oCNpc::EquipItem
    // @0x007323c0). OpenGothic skipped only equipped items, so unequipped armour carried
    // in a downed NPC's inventory became lootable; armour is never lootable from a body.
    while(at<it.size() && (it[at]->isEquipped() || it[at]->isArmor())) {
      ++at;
      }
    }
```
