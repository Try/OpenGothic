# Missing PERC_ASSESSFAKEGUILD broadcast when the player equips disguise armor

**Confidence:** High

## Original fn + address

`oCNpc::AddItemEffects` (Gothic2.exe `0x007320f0`) is invoked on every `oCNpc::EquipItem`
(`0x007323c0`). After it sums the item's protection/attribute bonuses, it handles the item's
*disguise guild*: it reads `oCItem::GetDisguiseGuild` (`0x007127e0`); if that value is non-zero
**and** differs from the NPC's true guild (`oCNpc::GetTrueGuild` `0x00730770`, field `0x766`), it
overwrites the live `C_Npc.guild` (`oCNpc::GetGuild` `0x00730750`, field `0x230`) with the disguise
guild. Crucially, inside that same branch it then performs an extra engine action that OpenGothic
omits:

- `CreatePassivePerception(self, 0x15 = PERC_ASSESSFAKEGUILD (21), self, NULL)` (`0x0075b270`),
  gated on `IsAPlayer` (`0x007425a0`, virtual vtable+0x104, `this == player`).

So when the hero dons faction armor whose disguise guild differs from his true guild, the engine
immediately broadcasts PERC_ASSESSFAKEGUILD to nearby NPCs so they re-assess his apparent guild on
the spot, rather than only after some later event. The perception fires for the player only.

## OG file:line

`/Users/admin/Downloads/opengothic/game/game/inventory.cpp:913-922` (`Inventory::applyArmor`, the
universal equip/unequip effects path, called with `sgn=1` on equip at line 481 and `sgn=-1` on
unequip at line 452).

## Divergence

OpenGothic already applies the disguise guild to `hnpc.guild` (and restores it on unequip — see the
existing NOTE at lines 906-912), but it never broadcasts the corresponding PERC_ASSESSFAKEGUILD.
Result: equipping a disguise (faction) armor changes the stored guild value, yet surrounding NPCs do
not get the passive perception that makes them re-evaluate the player's apparent guild at the moment
of equipping — their reaction is delayed until an unrelated perception happens to fire.

Verified the supporting symbols exist in OpenGothic:
- `PERC_ASSESSFAKEGUILD = 21` — `game/game/constants.h:430` (reachable via `gamescript.h`, included
  by `inventory.cpp:9`).
- `World::sendPassivePerc(Npc& self, Npc& other, int32_t perc)` — `game/world/world.h:170`,
  impl `game/world/world.cpp:706`. The (self, other) 2-NPC form matches the original's
  `(this, this, NULL)` arguments, exactly mirroring the existing PERC_DRAWWEAPON / PERC_ASSESSREMOVEWEAPON
  calls (`game/world/objects/npc.cpp:2042`, `:4036`).
- `Npc::isPlayer()`, `Npc::world()`, `Npc::trueGuild()` — all already used in this file.

## Proposed patch

OLD (`game/game/inventory.cpp`, in `Inventory::applyArmor`):
```cpp
  const int32_t disguise = it.handle().disguise_guild;
  if(disguise!=0) {
    auto& hnpc = owner.handle();
    if(sgn>0) {
      if(disguise!=owner.trueGuild())
        hnpc.guild = disguise;
      } else {
      hnpc.guild = owner.trueGuild();
      }
    }
```

NEW:
```cpp
  const int32_t disguise = it.handle().disguise_guild;
  if(disguise!=0) {
    auto& hnpc = owner.handle();
    if(sgn>0) {
      if(disguise!=owner.trueGuild()) {
        hnpc.guild = disguise;
        // NOTE: in original-game oCNpc::AddItemEffects (Gothic2.exe 0x007320f0), once an equipped
        // item's disguise_guild (!=0 and !=GetTrueGuild @0x00730770) overwrites C_Npc.guild, the
        // engine additionally fires CreatePassivePerception(self, PERC_ASSESSFAKEGUILD=21, self,
        // NULL) @0x0075b270 -- but only when IsAPlayer @0x007425a0 -- so nearby NPCs immediately
        // re-assess the hero's apparent guild on donning faction armor. OpenGothic set the guild
        // field but never broadcast the perception, so witnesses kept their old attitude until some
        // unrelated event.
        if(owner.isPlayer())
          owner.world().sendPassivePerc(owner,owner,PERC_ASSESSFAKEGUILD);
        }
      } else {
      hnpc.guild = owner.trueGuild();
      }
    }
```
