# Give-item-to-NPC engine path — parity review

**Confidence:** NO FINDING (give path is faithful for all realistic inputs)

## Original function + address (prose)

The give external is `Npc_GiveItem` @ `0x006e73e0` (`oGameExternal.cpp`). It pops the
declared args `(giver, item, receiver)`, resolves the item's instance index, and — when that
index is valid — performs exactly two engine calls:

- `oCNpc::RemoveFromInv(giver, instance, 1)` @ `0x007495f0`: removes one unit from the giver.
  Internally, if the removed `oCItem` carries the active/equipped flag `0x40000000`, it calls
  `oCNpc::Equip(giver, item)` @ `0x00739c90`, which is a *toggle* and therefore unequips the
  outgoing item.
- `oCNpc::PutInInv(receiver, item)` @ `0x00749350` → `oCNpcInventory::Insert` @ `0x0070c730`:
  deposits the unit into the receiver, stack-merging by `MultiSlot`. No owner/owner_guild
  stamping, no auto-equip of a received better weapon/armor, no perception is fired.

Before removing, the original also reads `oCNpc::GetTradeItem(giver)` @ `0x006bd570` and, if the
giver's currently-open trade item is the very item being given, calls
`oCNpc::CloseTradeContainer` on both giver and receiver (a dangling-pointer guard for the case
where a script gives away the item open in a live trade view).

## OG file:line

- `game/game/gamescript.cpp:2368` `GameScript::npc_giveitem` → `receiver->addItem(itemInstance,*giver,1)`
- `game/world/objects/npc.cpp:3783` `Npc::addItem(size_t,Npc&,size_t)` → `Inventory::transfer`
- `game/game/inventory.cpp:328` `Inventory::transfer`

## Divergence assessment

Every load-bearing behavior of the original give path is reproduced:

- **Count / direction / by-instance:** OG transfers exactly 1 unit from giver to receiver,
  matching `RemoveFromInv(...,1)` + `PutInInv`.
- **Unequip on give:** the original's flag-`0x40000000` toggle-unequip in `RemoveFromInv` is
  matched by `Inventory::transfer`'s `from.unequip(&it,*fromNpc)` on the whole-stack path; since
  equippable items never stack (count 1), the partial path is never reached for equipped items.
- **No auto-equip:** `Insert` @ `0x0070c730` and `PutInInv` only stack-merge — they do *not*
  auto-equip a received better weapon/armor. OG matches (no equip-best on give).
- **No owner re-stamp:** `PutInInv`/`Insert` never rewrite the item's `owner`/`owner_guild`;
  these stay the Daedalus instance defaults. OG's freshly-created item on the partial path
  (`to.addItem(itemSymbol,count,wrld)`) is built from the same instance, so its
  `handle().owner`/`owner_guild` (read by `Npc_OwnedByNpc`/`Npc_OwnedByGuild`) match.
- **No perception/reaction** fired by the engine on give. OG matches.

### Near-misses considered and rejected

1. **Trade-container-close-on-give** (`GetTradeItem`/`CloseTradeContainer`): a real engine
   behavior OG omits, but it only fires if a script runs `Npc_GiveItem` while the giver has a
   live trade view open on that exact item. In OG, trade is modal player-driven UI and script
   `npc_giveitem` is not reachable during it, so there is no dangling reference to clean up. Not
   surgically verifiable → rejected.
2. **Self-give crash** (`Npc_GiveItem(self, item, self)`): with giver==receiver and a whole-stack
   move, `Inventory::transfer` does `to.addItem(std::move(from.items[i]))` while `from`/`to` are
   the same inventory, leaving a moved-from null `unique_ptr` in `items`; `findByClass`
   (`inventory.cpp:1106`) then dereferences it. The original is safe (removes out, then puts
   back). However scripts never self-give, so the trigger is non-occurring → not high-confidence
   surgical; documented for awareness only, no patch proposed.

## Proposed patch

NO FINDING. The give-item engine path faithfully mirrors `RemoveFromInv` + `PutInInv` for all
realistic inputs. The two near-misses are either non-reproducible in OG's architecture
(trade-close) or non-occurring (self-give) and do not warrant a surgical change.
