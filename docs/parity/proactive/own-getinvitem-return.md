# Npc_GetInvItem returns symbol-index/-1 instead of a found/not-found boolean

**Confidence:** High

## Original function + address

`Npc_GetInvItem` is handled at **0x006eef00** (`oGameExternal.cpp`, the `_ulf`
external block; registered in `DefineExternals_Ulfi` as ordinal 0x34, two args,
returning int).

The handler pops the NPC and the item-instance id, calls the NPC inventory
`GetItem(instance)` once, and keeps the resulting `oCItem*` pointer in a
register. It then stores that pointer into the global `item` instance
(`zCParser::SetInstance("ITEM", item)`), and finally computes the return value
purely from whether the pointer is non-null: the epilogue does
`xor ecx,ecx; cmp esi,0; setne cl` and passes `ecx` (the item pointer) to the
integer `zCParser::SetReturn`. So the return is a strict boolean: **1 when the
item exists in the inventory, 0 when it does not** (the found item is exposed via
the global `item`, not via the return value).

Note the sibling external `Npc_GetInvItemBySlot` @0x006ef020 instead returns the
item *count* (`oCItem` field at +0x32C) — the two externals deliberately differ,
and OpenGothic already matches the count semantics for the by-slot variant.

## OpenGothic file:line

`/Users/admin/Downloads/opengothic/game/game/gamescript.cpp:2249` —
`GameScript::npc_getinvitem`.

## Divergence

OpenGothic stores the item correctly (`storeItem(itm)`) but returns
`int(itm->handle().symbol_index())` when the item is found and **`-1`** when it
is absent.

- Found path: original returns `1`; OpenGothic returns a large symbol index.
  Both are truthy, so plain `if(Npc_GetInvItem(...))` still passes — but any
  script comparing or arithmetically using the return value misbehaves.
- Not-found path: original returns `0` (falsy); OpenGothic returns `-1`, which is
  **truthy** in Daedalus. This inverts the common gate
  `if(Npc_GetInvItem(self, item)) { ... }`: it fires even when the NPC does not
  own the item.

## Proposed patch

Grep-verified symbols: `Npc::getItem(size_t)` (world/objects/npc.cpp:3834),
`GameScript::storeItem(Item*)` (game/gamescript.cpp:837).

OLD (game/game/gamescript.cpp:2249-2257):

```cpp
int GameScript::npc_getinvitem(std::shared_ptr<zenkit::INpc> npcRef, int itemId) {
  auto npc = findNpc(npcRef);
  auto itm = npc==nullptr ? nullptr : npc->getItem(uint32_t(itemId));
  storeItem(itm);
  if(itm!=nullptr) {
    return int(itm->handle().symbol_index());
    }
  return -1;
  }
```

NEW:

```cpp
int GameScript::npc_getinvitem(std::shared_ptr<zenkit::INpc> npcRef, int itemId) {
  // NOTE: in original-game Npc_GetInvItem @0x006eef00 stores the found item in the
  // global `item` instance and returns 1 when the item exists in the inventory, 0
  // otherwise (boolean via `setne` on the item pointer). OpenGothic returned the
  // item's symbol index when found and -1 when absent; -1 is truthy in Daedalus, so
  // `if(Npc_GetInvItem(self,it))` wrongly passed for items the NPC does not own.
  auto npc = findNpc(npcRef);
  auto itm = npc==nullptr ? nullptr : npc->getItem(uint32_t(itemId));
  storeItem(itm);
  return itm!=nullptr ? 1 : 0;
  }
```
