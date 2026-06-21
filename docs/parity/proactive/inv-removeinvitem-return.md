# Npc_RemoveInvItem / Npc_RemoveInvItems always return 0 instead of presence flag

**Confidence:** High

## Original function + address

The Daedalus externals `Npc_RemoveInvItem` and `Npc_RemoveInvItems` are declared
`func int`, and the original engine sets a meaningful return value.

- `Npc_RemoveInvItem` handler (Gothic2.exe `0x006e7d90`): resolves the NPC, calls
  `oCNpc::IsInInv(npc, item, 1)` to find an instance, calls
  `oCNpc::RemoveFromInv(npc, item, 1)`, and finishes with
  `zCParser::SetReturn(parser, (item-was-present) ? 1 : 0)` — i.e. it returns `1`
  when the item existed in the inventory (and was therefore removed), `0` when it
  did not.
- `Npc_RemoveInvItems` handler (Gothic2.exe `0x006e8200`): resolves the NPC, calls
  `oCNpc::IsInInv(npc, item, 1)`; if that finds nothing it returns `0`. Otherwise it
  removes the requested amount (multi-slot stackables in one bulk call when enough
  are present, otherwise unit-by-unit until exhausted — partial removal) and returns
  `1`. The success local is set to `1` on the path that reaches a removal and is left
  `0` only on the early "item not present" exit (`LAB_006e843b`).

In both cases the return value is purely a presence/success flag: `1` if at least
one matching item existed before the call, `0` if none did.

## OpenGothic file:line

- `game/game/gamescript.cpp:2210` — `GameScript::npc_removeinvitem` returns `0`
  unconditionally.
- `game/game/gamescript.cpp:2217` — `GameScript::npc_removeinvitems` returns `0`
  unconditionally.

## Divergence

OpenGothic performs the removal (`npc->delItem(...)`, which clamps over-removal to
emptying the stack, matching the original's partial-removal outcome), but both
externals always `return 0`. A Daedalus script that branches on the result —
`if(Npc_RemoveInvItems(self, ItMi_Gold, 100)) { ... }`, a common pattern for "did the
NPC actually have these items to take" gates — always takes the false branch in
OpenGothic, even when the items were present and removed. The original would return
`1` there.

`Inventory::itemCount(id)` (game/game/inventory.cpp:221, exposed as
`Npc::itemCount`, game/world/objects/npc.cpp:3660) returns the total stack count for
a class (equipped copies included, matching `IsInInv`), so `itemCount(id) > 0` is the
correct presence predicate for both externals.

## Proposed patch

NOTE citations reference the original handlers above.

`game/game/gamescript.cpp` — `npc_removeinvitem`:

OLD:
```cpp
int GameScript::npc_removeinvitem(std::shared_ptr<zenkit::INpc> npcRef, int itemId) {
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    npc->delItem(uint32_t(itemId),1);
  return 0;
  }
```

NEW:
```cpp
int GameScript::npc_removeinvitem(std::shared_ptr<zenkit::INpc> npcRef, int itemId) {
  // NOTE: in original-game Npc_RemoveInvItem @0x006e7d90 returns 1 when the item was
  // present in the inventory (and thus removed), 0 otherwise. OpenGothic always
  // returned 0, breaking scripts that gate on the result.
  auto npc = findNpc(npcRef);
  if(npc==nullptr)
    return 0;
  const bool had = npc->itemCount(uint32_t(itemId))>0;
  if(had)
    npc->delItem(uint32_t(itemId),1);
  return had ? 1 : 0;
  }
```

`game/game/gamescript.cpp` — `npc_removeinvitems`:

OLD:
```cpp
int GameScript::npc_removeinvitems(std::shared_ptr<zenkit::INpc> npcRef, int itemId, int amount) {
  auto npc = findNpc(npcRef);

  if(npc!=nullptr && amount>0)
    npc->delItem(uint32_t(itemId),uint32_t(amount));

  return 0;
  }
```

NEW:
```cpp
int GameScript::npc_removeinvitems(std::shared_ptr<zenkit::INpc> npcRef, int itemId, int amount) {
  // NOTE: in original-game Npc_RemoveInvItems @0x006e8200 returns 1 when at least one
  // matching item existed (and was removed; over-removal is a partial removal down to
  // empty), 0 when none existed. OpenGothic always returned 0.
  auto npc = findNpc(npcRef);
  if(npc==nullptr)
    return 0;
  const bool had = npc->itemCount(uint32_t(itemId))>0;
  if(had && amount>0)
    npc->delItem(uint32_t(itemId),uint32_t(amount));
  return had ? 1 : 0;
  }
```

Notes on parity edge cases (already consistent, no change needed):
- The original removes as many as available even if `amount` exceeds stock (partial
  removal); OpenGothic's `Inventory::delItem` clamps the same way
  (`it->count()>count ? -count : 0`), so the removal effect already matches.
- The return is a presence flag, not the count removed; `itemCount>0` reproduces it
  exactly for both stackable and non-stackable items, since the original's gating
  `IsInInv(item,1)` likewise counts equipped copies.
