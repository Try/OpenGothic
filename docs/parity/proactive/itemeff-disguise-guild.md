# Item on-equip disguise-guild not applied (armor disguise)

**Confidence:** Medium-High (engine behavior unambiguous from decompilation; observable impact depends on content using `disguise_guild`, but the equip/unequip guild swap is a clear engine-level parity gap).

## Original function + address

`oCNpc::AddItemEffects` (Gothic2.exe `0x007320f0`) is the generic on-equip
effect routine called from `oCNpc::EquipItem` (`0x007323c0`) for *every*
equipped item. Besides the per-item `change_atr[]`/`change_value[]` attribute
bonuses (3 entries, skipped when `change_atr<=0`) and the 8 `protection[]`
additions, it also applies the item's *disguise guild*:

- It reads `oCItem::GetDisguiseGuild` (`0x007127e0`, returns the item's
  `disguise_guild` field at item+0x208).
- If the disguise guild is non-zero **and** differs from the NPC's *true* guild
  (oCNpc+0x766), it overwrites the NPC's live guild (the script-visible
  `C_Npc.guild`, oCNpc+0x230) with the disguise guild. For the player it then
  fires `CreatePassivePerception(0x15)` so nearby NPCs re-assess.

`oCNpc::RemoveItemEffects` (`0x00732270`), called from `oCNpc::UnequipItem`
(`0x007326c0`), performs the inverse: if the item's `disguise_guild` is
non-zero, it restores the live guild to the true guild (oCNpc+0x766).

Net effect: wearing armor whose `disguise_guild` is set makes other NPCs read
your *guild* (and thus guild attitudes) as the disguise faction, while monster/
human classification and other "true guild" logic stay tied to the real guild.

## OpenGothic file:line

`game/game/inventory.cpp:891` — `Inventory::applyArmor(Item&, Npc&, int32_t sgn)`
(the on-equip/un-equip stat hook; called for every slot through `setSlot`).
It applies `protection[]` and the `change_atr[]/change_value[]` bonuses but
never touches the disguise guild. A grep of the whole `game/` tree shows
`disguise_guild` is only ever read/written for serialization
(`world/objects/item.cpp:54,124`); no code path applies it on equip.

The supporting infrastructure already exists and even anticipates this:
`Npc::trueGuild()` / `Npc::setTrueGuild()` (`world/objects/npc.cpp:1336-1344`),
and the #656 note at `npc.cpp:1312-1315` explicitly switches `isMonster`/
`isHuman` to the *true* guild "so a runtime guild change (disguise) wrongly
flipped monster/human status" — i.e. the design expects disguise to mutate the
live `C_Npc.guild`, but nothing ever performs that mutation.

## Divergence

In OpenGothic, equipping disguise armor does not change `hnpc->guild`, so
`GameScript::guildAttitude` (`gamescript.cpp:1420-1422`, keyed on
`Npc::guild()` → `hnpc->guild`) keeps returning the player's real-guild
attitude. The disguise has no gameplay effect; in the original the same item
makes the disguised faction non-hostile (and reverts on unequip).

## Proposed patch

`applyArmor` already has the `sgn` direction and mutable handle access (it uses
`owner.handle()` in `applyWeaponStats`). Add a disguise block mirroring
AddItemEffects/RemoveItemEffects. The `disguise_guild!=0` guard makes this a
no-op for the common armor/ring/amulet (all `disguise_guild==0`).

OLD (`game/game/inventory.cpp`, end of `applyArmor`):
```cpp
  for(size_t i=0;i<zenkit::IItem::condition_count;++i){
    const int32_t atr = it.handle().change_atr[i];
    if(atr>0)
      owner.changeAttribute(Attribute(atr), it.handle().change_value[i]*sgn, false);
    }
  }
```

NEW:
```cpp
  for(size_t i=0;i<zenkit::IItem::condition_count;++i){
    const int32_t atr = it.handle().change_atr[i];
    if(atr>0)
      owner.changeAttribute(Attribute(atr), it.handle().change_value[i]*sgn, false);
    }

  // NOTE: in original-game oCNpc::AddItemEffects (Gothic2.exe 0x007320f0) /
  // RemoveItemEffects (0x00732270) an equipped item with a non-zero
  // disguise_guild (oCItem::GetDisguiseGuild 0x007127e0) overwrites the live
  // C_Npc.guild: on equip with the disguise guild (only if it differs from the
  // true guild), on unequip back to the true guild. This is how worn faction
  // armor changes guild attitudes while monster/human stays on the true guild
  // (#656). OpenGothic serialized disguise_guild but never applied it.
  const int32_t disguise = it.handle().disguise_guild;
  if(disguise!=0) {
    auto& hnpc = owner.handle();
    if(sgn>0) {
      if(disguise!=owner.trueGuild())
        hnpc.guild = disguise;
      }
    else {
      hnpc.guild = owner.trueGuild();
      }
    }
  }
```

Grep-verified OG symbols: `Item::handle()` / `it.handle().disguise_guild`
(`world/objects/item.cpp:54`), `Npc::handle()` returns mutable `zenkit::INpc&`
(`world/objects/npc.h:337`), `INpc::guild` (`npc.cpp:1305,1342`),
`Npc::trueGuild()` (`npc.h:226`), `sgn` parameter of `applyArmor`.

Out of scope / deferred sub-part: the player-only
`CreatePassivePerception(0x15)` immediate re-assessment broadcast — NPCs already
re-evaluate attitude on their normal perception cycle, so the guild swap takes
effect without it; adding the broadcast would need the matching PERC id wiring
and is a separate, riskier change.
