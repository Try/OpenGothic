# Armor equip does not apply `disguise_guild` (NPC stays its true guild while disguised)

**Confidence:** High (divergence is unambiguous and grep-verified; vanilla-observable only with armor that sets a non-zero `disguise_guild`, which is mod / special-content territory rather than a guaranteed vanilla repro).

## Original function + address (prose only)

When the original engine equips an item it runs `oCNpc::AddItemEffects` (`Gothic2.exe @0x007320f0`); when it un-equips it runs `oCNpc::RemoveItemEffects` (`@0x00732270`). These are the exact two functions the existing OpenGothic `applyArmor` NOTE comment already maps itself onto (protection[] add/subtract and change_atr boni).

Besides protection and the attribute boni, `AddItemEffects` reads the item's disguise guild (`oCItem::GetDisguiseGuild`). If that value is non-zero **and** differs from the NPC's *true* guild (the byte at `oCNpc+0x766`, i.e. OpenGothic's `trueGuild()`), it overwrites the NPC's *live* guild field (`oCNpc+0x230`, the `C_Npc.guild` that everything else reads) with the disguise guild; for the player it additionally fires a passive perception so nearby NPCs re-assess. `RemoveItemEffects` does the mirror: if the item's disguise guild is non-zero it restores the live guild to the true guild (`oCNpc+0x766`).

This is precisely the true-guild / live-guild split OpenGothic already models (see the `#656` notes around `oCNpc::IsMonster`/`IsHuman` in `npc.cpp`): combat classification reads `trueGuild()`, while attitude/perception reads the live `C_Npc.guild`. The disguise mechanic is the one engine path that is supposed to make the two differ — and it is the only one not wired up.

## OpenGothic file:line

`/Users/admin/Downloads/opengothic/game/game/inventory.cpp:866` — `Inventory::applyArmor(Item&, Npc&, int32_t sgn)` (called from `setSlot` for every equip/unequip, `+1` on equip, `-1` on unequip — the same scope as `AddItemEffects`/`RemoveItemEffects`).

## Divergence

`applyArmor` applies `protection[]` and `change_atr[]` but never touches the guild. `disguise_guild` is only ever round-tripped through save/load (`/Users/admin/Downloads/opengothic/game/world/objects/item.cpp:54,124`) and is otherwise dead — `grep -rn "disguise_guild\|disguise" game/` shows no application site. Consequently a disguise armor never changes how other NPCs perceive the wearer (guild-based attitude / passive perception), whereas in the original the wearer is treated as a member of the disguise guild until the armor comes off.

Verified symbols: `zenkit::IItem::disguise_guild` (daedalus.hh:296), `zenkit::INpc::guild` (daedalus.hh:196), `Npc::handle()` → mutable `INpc&` (npc.h:336), `Npc::trueGuild()` (npc.h:226, public), `Npc::guild()` reads `hnpc->guild` (npc.cpp:1298).

## Proposed patch

In `Inventory::applyArmor`, after the existing protection / change_atr loops, before the closing brace:

OLD:
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

  // NOTE: in original-game oCNpc::AddItemEffects (Gothic2.exe @0x007320f0) / RemoveItemEffects
  // (@0x00732270) an equipped item with a non-zero disguise_guild swaps the NPC's live guild
  // (C_Npc.guild, oCNpc+0x230) to that guild on equip -- but only when it differs from the true
  // guild (oCNpc+0x766) -- and restores it to the true guild on unequip. OpenGothic serialized
  // disguise_guild but never applied it, so a disguise armour never changed guild-based attitude
  // / perception. (Perception re-assess on the player is intentionally left out as a separate
  // step; the next perception tick re-evaluates from the updated guild.)
  const int32_t disguise = it.handle().disguise_guild;
  if(disguise!=0) {
    if(sgn>0) {
      if(disguise!=owner.trueGuild())
        owner.handle().guild = disguise;
      } else {
      owner.handle().guild = owner.trueGuild();
      }
    }
  }
```

Non-armor equips keep `disguise_guild==0`, so the new block is a no-op for them — matching `AddItemEffects`'s generic (all-items) scope.
