# C_ITEM.wear equip-slot routing (WEAR_TORSO / WEAR_HEAD) is parsed but ignored

**Confidence:** Medium. The parsed-but-ignored divergence is certain and
decompiler-verified; the field is provably unused in OpenGothic. Scored Medium
(not High) only because the faithful fix is a feature (a head-attachment equip
slot OpenGothic does not model) and retail Gothic II armor instances all use
`WEAR_TORSO`, so vanilla manifestation is limited — hence **DEFERRED**.

## Original function + address

`oCNpc::EquipArmor` (entry `0x0073a490`, source tag `oNpc.cpp`) and
`oCNpc::EquipItem` (entry `0x007323c0`) both branch on the equipped item's
`wear` field, read from the item instance at offset `+0x18c` (the int that sits
just after the `value`/`damage` block, mirroring `C_ITEM.wear`):

- `wear & 2` (`WEAR_HEAD`)  -> selects the head-node body slot (a fixed
  slot-name string global; the torso counterpart is the `NPC_NODE_TORSO` node
  that `oCNpc::InitModel` @`0x00738480` queries via `GetSlotItem`).
- `wear & 1` (`WEAR_TORSO`) -> selects the torso body slot.
- neither bit set -> the engine emits the error
  `"U: NPC: (WearItem) Ungueltiger 'wear'-Wert von <item>"` (string @`0x008b8920`,
  referenced from `EquipArmor`) and the slot-name string is left empty, so the
  guarded `EquipItem`/`PutInSlot` call is skipped: **the item is not equipped**.

So `wear` is the knob that (a) routes a worn item to the torso vs the head
attachment slot — finding and unequipping whatever item already occupies that
*same* slot before equipping the new one — and (b) hard-refuses any item whose
`wear` value carries neither slot bit.

## OpenGothic file:line

- `game/world/objects/item.cpp:52,122` — `wear` is read/written **only** in
  (de)serialization (`fin.read(h->wear,...)` / `fout.write(h.wear,...)`).
- `game/game/inventory.cpp:1077` (`Inventory::equipArmor`) and the armor branch
  of `Inventory::use` / `Npc::updateArmor` (`game/world/objects/npc.cpp:945`):
  all armor is routed through a single torso/body slot with no reference to the
  item's `wear` field.

A repo-wide grep for the field (`grep -rn '\bwear\b' game/`) finds it only at
the two `item.cpp` serialization lines; every other `wear`/`wear-slot` hit in
`inventory.cpp` refers to the AMULET/RING/BELT `ItemFlag` bits, not
`IItem::wear`. `IItem::wear` is grep-verified to exist
(`lib/ZenKit/include/zenkit/addon/daedalus.hh:283`).

## Divergence

OpenGothic never consults `C_ITEM.wear`. Consequences vs. the original:

1. A `WEAR_HEAD` item (helmet/mask/circlet) is attached to a head node and
   occupies a separate equip slot in the original; OpenGothic has no head
   armor/attachment slot, so such an item is mis-routed (treated as torso
   armor) or not visually placed where the original puts it.
2. An item whose `wear` carries neither slot bit (e.g. left at `0`) is
   **refused** by the original (logged invalid, not equipped) but is silently
   equipped by OpenGothic.

## Proposed patch — DEFERRED

No surgical, build-verifiable fix. Faithful parity needs a head-attachment
equip slot (the `WEAR_HEAD` branch) that OpenGothic's single torso-slot armor
model does not provide; implementing it is a feature, not a local edit. The
narrower "validate `wear` and refuse items with neither slot bit" change is
risky in the clean-room sense: it would silently un-equip any content whose
armor instance leaves `wear == 0`, and retail Gothic II armor uniformly sets
`wear = WEAR_TORSO`, so the change has no positive vanilla effect while adding a
regression surface for mods/edge content.

```
// NOTE: in original-game oCNpc::EquipArmor @0x0073a490 / oCNpc::EquipItem
// @0x007323c0 the item's C_ITEM.wear field (item+0x18c) selects the equip slot
// -- WEAR_HEAD(0x2) -> head node, WEAR_TORSO(0x1) -> torso node -- and an item
// with neither bit is rejected ("Ungueltiger 'wear'-Wert"). OpenGothic ignores
// IItem::wear entirely (read only in item.cpp serialization) and routes all
// armor through one torso slot. DEFERRED: faithful behavior requires a head
// equip slot OpenGothic does not model.
```
