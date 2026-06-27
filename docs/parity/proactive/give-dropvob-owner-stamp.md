# Dropped items are stamped with an NPC `owner` on drop; the original never sets owner on drop

**Confidence:** Medium (DEFERRED — divergence is real and binary-confirmed, but a surgical fix is not yet
isolable because OpenGothic's runtime item-ownership model differs from the original wholesale).

## Original function + address

When an NPC (or the player) drops a held item into the world, the engine runs
`oCNpc::DoDropVob` (Gothic2.exe `@0x00744dd0`); the slot-clearing variant is
`oCNpc::DropFromSlot` (`@0x0074a660`). Both routines were inspected in full. Neither writes the
`oCItem` owner fields — the per-item owner NPC index (`oCItem+0x200`, read by
`oCItem::IsOwnedByNpc` `@0x007127a0`) and owner-guild (`oCItem+0x204`,
`oCItem::IsOwnedByGuild` `@0x007127c0`) are left untouched by the drop. The only thing the drop
paths toggle on the item is an engine-internal flag `0x400`: `DoDropVob`/`DropFromSlot` *set*
flag `0x400` when the dropper is the player (the virtual taker/dropper test at NPC vtable
`+0x104`), and the matching pick-up routine `oCNpc::DoTakeVob` (`@0x007449c0`) *clears* flag
`0x400` when the taker is not the player. (No reader of flag `0x400` was located on the
ownership/theft/save paths, so it appears to be a player-dropped marker with no observable
consumer in the stock engine.)

The companion checks confirm the original keeps drop and ownership orthogonal:
`oCNpcInventory::Insert` (`@0x0070c730`) does not set item owner on inventory insert,
`oCNpcInventory::SetOwner` (`@0x0070c320`) only stores an owner pointer on the *inventory*
container (not on the items), and `oCItem::Unarchive` (`@0x00713eb0`) does not even restore an
item-level owner. Net: in the original, the runtime drop/take cycle never assigns
`oCItem` NPC-ownership.

## OpenGothic file:line

`game/world/objects/npc.cpp:3590` (in `Npc::dropItem`):

```cpp
auto it = owner.addItemDyn(id,drop,hnpc->symbol_index());
```

and the sink `game/world/objects/worldobjects.cpp:688` (in `WorldObjects::addItemDyn`):

```cpp
it->handle().owner = ownerNpc==size_t(-1) ? 0 : int32_t(ownerNpc);
```

The other `addItemDyn` callers do the same stamping with the acting NPC's symbol:
`npc.cpp:871` (torch drop) and `mdlvisual.cpp:293`/`mdlvisual.cpp:314` (thrown/placed item).

## Divergence

OpenGothic stamps every item dropped/thrown into the world with
`handle().owner = <dropping NPC's symbol_index>`. The original drop paths
(`oCNpc::DoDropVob` / `oCNpc::DropFromSlot`) never write item owner. Consequence:
after an NPC drops an item (e.g. via `AI_DropItem`), OpenGothic's
`GameScript::npc_ownedbynpc` (`gamescript.cpp:2875`, which compares
`itm->handle().owner` against the queried NPC) reports the dropped item as owned by the
dropper, whereas in the original that item is unowned at the item level. For player drops the
item is stamped owned-by-hero, which the original instead represents only as the inert flag
`0x400`. The two engines therefore disagree on item ownership for any item that entered the
world through a runtime drop.

## Proposed patch

**DEFERRED.** Although the divergence is concrete, a high-confidence surgical fix cannot be
proposed yet:

1. OpenGothic appears to assign runtime item NPC-ownership *only* at drop time (the sole
   writer of `handle().owner` outside save-load is `addItemDyn`). The original assigns
   item ownership through a different mechanism entirely (and `Npc_OwnedByNpc` itself is
   dispatched original-side as an NPC method `oCNpc::vtbl[+0x84](item.instance)`, not an
   item-field compare). Simply dropping the owner stamp would make OpenGothic-dropped items
   unowned to match `DoDropVob`, but it could regress OpenGothic's own theft/loot bookkeeping
   that may currently lean on the drop-time stamp, and it would not by itself reconcile the
   deeper `npc_ownedbynpc` semantic mismatch.
2. The player-side half (flag `0x400` set on player drop, cleared on non-player take) has no
   located reader in the stock engine, so reproducing it has no verifiable behavioral payoff.

Resolving this safely requires first establishing where OpenGothic intends item NPC-ownership
to originate (creation vs. drop) and aligning `npc_ownedbynpc` with the original's NPC-side
dispatch, then deciding whether `Npc::dropItem`/`addItemDyn` should stop stamping owner. Until
that model is settled, no surgical change is build-verifiably correct.

// NOTE: in original-game oCNpc::DoDropVob @0x00744dd0 and oCNpc::DropFromSlot @0x0074a660 do
// not write oCItem owner (0x200/0x204) on drop; oCNpc::DoTakeVob @0x007449c0 only toggles the
// internal player-dropped flag 0x400. OpenGothic instead stamps handle().owner on every drop.
