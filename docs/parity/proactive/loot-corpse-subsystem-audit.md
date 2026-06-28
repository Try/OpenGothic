# Loot — dead/unconscious-NPC body looting audit

**Result: NO FINDING** (corpse-looting engine path verified faithful)

**Confidence:** High that the audited paths match; no surgical divergence found.

## Scope

Audited the full dead/unconscious-NPC body-loot path against `Gothic2.exe`:

- Dispatch / eligibility: `oCAIHuman::StandActions` @0x00698ea0 → `oCNpc::OpenDeadNpc`
  @0x00762970.
- Loot-list contents: `oCNpcContainer::CreateList` @0x0070b570.
- Take / transfer: `oCNpcContainer::HandleEvent` @0x0070b6f0, `oCNpc::CanCarry`
  @0x00749220 / `oCNpcInventory::CanCarry` @0x0070f4d0, `oCNpc::CloseDeadNpc` @0x00762b40.
- Downed-state side effects: `oCNpc::DropUnconscious` @0x00735eb0 / `DropAllInHand` @0x007375e0.

OG references: `game/ui/inventorymenu.cpp` (`ransack`, `onTakeStuff`, `tick`),
`game/game/inventory.cpp` (`Iterator::skipHidden` T_Ransack, `transfer`),
`game/game/playercontrol.cpp:340` (`interact(Npc&)`),
`game/world/objects/npc.cpp` (`isDown`, `addItem`).

## Findings (all faithful)

1. **Eligibility (dead vs unconscious).** Original `OpenDeadNpc`/`StandActions` gate looting on
   `IsDead(npc) || IsUnconscious(npc)`. OG `interact(Npc&)` routes on `other.isDown()`, and
   `Npc::isDown()` = `isUnconscious() || isDead()` (npc.cpp:4573). Match. Unconscious NPCs go
   through the free-loot path (not the steal/pickpocket path) in both.

2. **Loot-list contents.** `CreateList` inserts an item only when `HasFlag(0x10)==0`
   (ITM_CAT_ARMOR) AND `HasFlag(0x40000000)==0` (equipped). `0x40000000` confirmed equipped
   (only set/cleared by Equip*/PutInSlot/Unequip* paths via `wde find 0x40000000`). OG
   `Iterator::skipHidden` T_Ransack skips `isEquipped() || isArmor()`, and `Item::isArmor()`
   tests `ITM_CAT_ARMOR` (0x10, item.cpp:301). Match — this is the already-applied armor-exclusion
   fix. Gold (neither armor nor equipped) is included in both.

3. **Equipped-items-on-corpse.** Equipped (sheathed) weapons/armor are excluded from the list in
   both; `HandleEvent`'s unequip-on-take branch is a safety net that is unreachable for the corpse
   list, and OG's `Inventory::transfer` (inventory.cpp:345) likewise unequips before moving.
   `DropUnconscious` only drops the **in-hand drawn** weapon (`DropAllInHand`), leaving sheathed
   equipment equipped — so equipped gear is non-lootable in both. Match.

4. **Take-amount step function.** `HandleEvent` maps the transfer counter to 1/10/100/1000/10000
   (capped at 10000) and accumulates the counter *by the amount*, resetting on stack-empty. OG
   `onTakeStuff` Normal-mode reproduces this exactly (inventory.cpp Normal branch, takeCount += amount;
   reset to 0 on empty). Match — already-applied TransferCountToAmount fix.

5. **Close-on-empty.** `HandleEvent` calls `CloseDeadNpc` once the list empties; OG `tick()` closes
   Ransack when the T_Ransack iterator is no longer valid, and also when the trader stops being
   `isDown()`. Match.

6. **No theft on body-loot.** `OpenDeadNpc` performs no AssessTheft / witness check; looting a dead
   or unconscious body is always free. OG `onTakeStuff` Ransack branch does a plain `addItem` with
   no theft path. Match.

7. **CanCarry cap.** Original gates each take on `CanCarry`, whose only real limit is
   `(item-stack count) < 0x400` (1024 distinct stacks); unreachable in normal play. OG omits this
   cap. Negligible / not surgical.

## Conclusion

The corpse-looting engine logic is faithful. The only candidate-adjacent observation
(`InventoryMenu::drawAll` skipping the player panel in `State::Ransack` while `OpenDeadNpc` calls
`OpenInventory`) is UI layout, not engine behavior, and matches the visible single-list dead-loot
screen. Per "empty beats false positives," no surgical fix is proposed.
