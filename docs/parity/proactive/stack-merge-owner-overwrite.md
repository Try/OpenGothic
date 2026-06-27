# Stack-merge wrongly overwrites item owner / owner_guild

**Confidence:** High

## Original function + address

`oCNpcInventory::Insert(oCItem*)` — Gothic2.exe @ `0x0070c730`.

Behavior (in prose, observed from the warm decompiler):

1. Reject a null item; if the item is currently in the world, remove it from
   the world first; validate it has a real instance index and a name.
2. **Stack-merge branch.** If the incoming item is stackable
   (`oCItem::MultiSlot()` @ `0x007125a0` returns non-zero), it walks the
   existing inventory list. For each existing entry that is *also* stackable
   and has the *same instance index* (virtual `GetInstance`, slot 0x78), it
   merges by doing exactly two things:
   - adds the incoming item's count to the existing entry's count
     (field at item+0x32c, the amount/count field), and
   - asks `ogame` to destroy the incoming item, then returns the **existing**
     entry.
   It does **not** copy any other field from the incoming item onto the
   existing stack — in particular it never touches the existing entry's
   `owner` or `owner_guild`. The surviving stack keeps whatever owner/guild it
   already had.
3. Otherwise it dedups by pointer and inserts the item as a new sorted list
   node (again without modifying owner/owner_guild of anything).

So in the original, a stack-merge preserves the *existing* stack's
`owner`/`owner_guild`; the incoming item's ownership data is simply discarded
together with the destroyed incoming item.

`owner` is read by `Npc_OwnedByNpc` (theft / ownership checks), so the value
that survives a merge is behaviorally observable.

## OpenGothic file:line

`game/game/inventory.cpp:228-246`, function `Inventory::addItem(std::unique_ptr<Item>&& p)`:

```
} else {
    it->setCount(it->count()+p->count());
    it->handle().owner       = p->handle().owner;
    it->handle().owner_guild = p->handle().owner_guild;
    return p.get();
    }
```

This overload is reached on world-item pickup (`Npc::implPickItem`,
`game/world/objects/npc.cpp:3490`) and on whole-item inventory transfer
(`Inventory::transfer`, `game/game/inventory.cpp:341`). Picked-up world items
carry a meaningful `owner` (set in `WorldObjects`, `game/world/objects/worldobjects.cpp:688`),
so the overwrite is live.

## Divergence

On every stack-merge OpenGothic overwrites the surviving stack's `owner` and
`owner_guild` with the *incoming* item's values. The original engine never
modifies those fields on a merge; it keeps the existing stack's owner/guild and
discards the incoming item's. Example: the player already holds a legitimately
owned stack of a stackable item (owner = 0) and then picks up another,
NPC-owned copy of the same instance. In the original the merged stack stays
owner = 0; in OpenGothic the whole stack is retagged with the NPC owner, which
flips later `Npc_OwnedByNpc` ownership/theft results.

## Proposed patch

Remove the two owner-copy lines so the merge only sums the count, matching the
original.

OLD (`game/game/inventory.cpp`, addItem(unique_ptr) merge branch):
```cpp
    } else {
    it->setCount(it->count()+p->count());
    it->handle().owner       = p->handle().owner;
    it->handle().owner_guild = p->handle().owner_guild;
    return p.get();
    }
```

NEW:
```cpp
    } else {
    // NOTE: in original-game oCNpcInventory::Insert @0x0070c730 a stack-merge only
    // sums the count and destroys the incoming item; it never copies owner/owner_guild
    // onto the surviving stack, so the existing stack keeps its ownership.
    it->setCount(it->count()+p->count());
    return p.get();
    }
```

Verified OG symbols: `Item::handle()` (mutable overload) `game/world/objects/item.h:89`;
`zenkit::IItem::owner` / `owner_guild` (serialized at `game/world/objects/item.cpp:54,124`);
`Item::setCount`/`Item::count` used elsewhere in this file. The removed lines reference
only existing symbols, so the change compiles.
