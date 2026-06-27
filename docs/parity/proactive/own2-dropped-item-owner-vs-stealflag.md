# own2 — Dropped-item ownership: OpenGothic stamps `C_ITEM.owner`, original uses the 0x400 player steal-flag instead

**Confidence:** Medium (clear, grep-verified code divergence; observable in-game impact is plausible but narrow)

## Original fn + address (prose)

- `oCNpc::DoDropVob` @ `0x00744dd0` is the engine routine that moves a vob/item from an NPC's
  inventory or hand back into the world. It never writes the item's `owner` (oCItem field @ `+0x200`)
  or `owner_guild` (@ `+0x204`). The only ownership-relevant thing it does is: after testing the
  virtual `IsAPlayer`-style predicate (vtbl `+0x104`), **if the dropper is the player it calls
  `oCItem::SetFlag(item, 0x400)`** — i.e. it stamps a runtime "steal/player-dropped" flag on the
  world item, leaving `owner`/`owner_guild` untouched. (The other flag it touches, `0x40000000`,
  is the equipped flag and is unrelated.)
- `oCNpc::DoTakeVob` @ `0x007449c0` is the take-from-world routine. It likewise never writes
  `owner`/`owner_guild`. The mirror of the drop logic is: **if the taker is NOT the player it calls
  `oCItem::ClearFlag(item, 0x400)`** (clearing the player-dropped marker when an NPC picks up the
  item), and it fires `CreatePassivePerception(this, 0x11 /*PERC_ASSESSTHEFT*/, this, NULL)` after a
  successful `PutInInv`.
- Ownership predicates that the theft scripts read: `oCItem::IsOwnedByNpc` @ `0x007127a0`
  (`owner>0 && owner==arg`, field @ `+0x200`) and `oCItem::IsOwnedByGuild` @ `0x007127c0`
  (`owner_guild==arg`, field @ `+0x204`).

So in the original the *individual-NPC* `owner` field of a world item is set only from the Daedalus
item instance constructor; the take/drop paths never stamp it at runtime. Player-dropped-loose-loot
is tracked exclusively via the oCItem runtime flag `0x400`.

## OG file:line

- `game/world/worldobjects.cpp:695` — `it->handle().owner = ownerNpc==size_t(-1) ? 0 : int32_t(ownerNpc);`
  inside `WorldObjects::addItemDyn`.
- Callers that pass the dropping NPC's symbol index as `ownerNpc`:
  - `game/world/objects/npc.cpp:3752` — `Npc::dropItem` (AI / `Npc_DropItem`)
  - `game/world/objects/npc.cpp:878` — burning-torch drop in `toggleTorch`
  - `game/graphics/mdlvisual.cpp:293` and `:314` — weapon/shield drop on death
- The stamped field is read for theft by `GameScript::npc_ownedbynpc` (`game/game/gamescript.cpp:3097`).
- OG has no analogue of the `0x400` flag, and `Npc::takeItem` (`game/world/objects/npc.cpp:3633`)
  performs no ownership clear on take.

## Divergence

OpenGothic stamps `C_ITEM.owner` (the *individual-NPC* owner used by `Npc_OwnedByNpc`) onto **every**
dynamically dropped item, for both the player and NPCs. The original `DoDropVob`/`DoTakeVob` never
write `owner`/`owner_guild`; they instead set/clear the oCItem `0x400` player steal-flag. Concretely:

- Player drops a normal item, then re-takes it: OG now has `owner == hero` and (with ASSESSTHEFT
  firing for the player) routes through `B_AssessTheft`/`Npc_OwnedByNpc(item, hero)`; the original
  would have `owner` unchanged and the `0x400` flag set.
- An NPC drops an item (torch via `toggleTorch`, or weapon/shield on death): OG marks it
  `owner == thatNpc`, so a subsequent player pickup can be assessed as owned-by-NPC, whereas in the
  original the dropped item carries no runtime `owner` and the `0x400` flag was just cleared.

Because OG also re-creates the world item from its Daedalus instance on drop (`new Item(... clsId ...)`),
any runtime-stamped `owner_guild` from a prior theft would additionally be reset to instance defaults —
a second, structural difference in the same path.

## Proposed patch — DEFERRED

Two reasons this is not a surgical high-confidence one-liner:

1. **No field to port the original behavior to.** Faithful parity means modelling the oCItem `0x400`
   runtime steal-flag (set on player drop, cleared on NPC take) rather than the `owner` field. zenkit's
   `IItem`/`C_ITEM` has no such runtime flag, so replicating the original requires adding a
   non-serialized runtime member plus identifying every reader of `0x400` in `Gothic2.exe` (its
   downstream effect — focus/naming vs. theft gating — is not yet established). That is out of scope
   for a surgical fix.

2. **Removing the OG `owner` stamping risks a real-but-different regression.** Simply matching the
   original by dropping line `worldobjects.cpp:695` (candidate below) would also remove whatever
   theft-on-loot behavior OG currently derives from dropped-item `owner`, and OG's own ASSESSTHEFT
   gating (`isPlayer` in `takeItem`, separately deferred) interacts with it. Confidence that this is
   net-positive in OG is only Medium, so per "empty beats false positives" it should not be applied
   blindly.

Candidate (for reference only, NOT to apply without play-testing the theft scripts):

```
// OLD — game/world/worldobjects.cpp:695 (WorldObjects::addItemDyn)
it->handle().owner = ownerNpc==size_t(-1) ? 0 : int32_t(ownerNpc);

// NEW
// NOTE: in original-game oCNpc::DoDropVob @0x00744dd0 and oCNpc::DoTakeVob @0x007449c0 the
// runtime drop/take paths never write C_ITEM.owner (+0x200) or owner_guild (+0x204); player-dropped
// loose loot is tracked via the oCItem 0x400 steal-flag (SetFlag on player drop, ClearFlag on NPC
// take), which OpenGothic does not model. Do not fabricate per-NPC ownership on drop.
(void)ownerNpc; // owner left at instance default
```
