# Npc_GiveItem external is unbound: item is never transferred between NPCs

**Confidence:** Medium-High. The divergence (the external is not bound, so the call silently
no-ops) is certain and grep-verifiable; the original transfer behavior is fully decompiled. The
residual uncertainty is (a) vanilla G2/NotR scripts rarely call this external — `B_GiveInvItems`
uses `CreateInvItems`+`Npc_RemoveInvItems` instead — so the in-game impact is mostly on mods, and
(b) the giver/receiver parameter mapping is derived from the decompiled stack-pop order rather than
a script-side declaration.

## Original function + address (prose)

`Npc_GiveItem` is implemented by the engine external thunk at **Gothic2.exe @0x006e73e0**
(`oGameExternal.cpp`, registered via `DefineExternals_Ulfi`; the `'Npc_GiveItem'` literal lives at
0x008b5648). Its Daedalus signature is `(c_npc, int item, c_npc)`: the engine pops one NPC, then an
int item-instance, then a second NPC off the parser stack. Because Daedalus pops arguments in
reverse declaration order, the first-declared NPC is the **giver** and the last-declared NPC is the
**receiver** (the middle int is the item instance).

Behavior, in order:
1. Resolve the item argument. If the passed symbol is actually an `oCItem` instance (RTTI cast to
   `oCItem` succeeds because its class is `ITEM`), it is replaced by that item's instance id; otherwise
   the raw int instance id is used. If the resulting id is negative the call does nothing.
2. If the **receiver's** current trade item (`oCNpc::GetTradeItem`) is the very item being given
   (instance ids match), it calls `oCNpc::CloseTradeContainer` on both the receiver and the giver
   (so an open trade window referencing that item is dismissed).
3. `poItem = oCNpc::RemoveFromInv(giver, itemId, 1)` — removes exactly **one** unit from the giver
   (the count `1` is a hard-coded literal; Npc_GiveItem never transfers more than one unit per call).
   `RemoveFromInv` (@0x007495f0) additionally re-`Equip`s the giver with the returned item if it
   still carries the equipped flag `0x40000000` (only relevant when a partial stack is left behind).
4. `oCNpc::PutInInv(receiver, poItem)` — deposits that unit into the receiver's inventory
   (`PutInInv` @0x00749350 merges into an existing stack; it does **not** auto-equip the receiver).

## OpenGothic file:line

- `game/game/gamescript.cpp` — the `bindExternal(...)` block (lines ~111-245) registers no
  `npc_giveitem`. A repo-wide, case-insensitive search for `giveitem` over all `.cpp/.h/.cc`
  matches nothing.
- `game/gothic.cpp:964` — `vm.register_default_external([](std::string_view name){ notImplementedRoutine(...); });`
  is the only handler an unbound `Npc_GiveItem` reaches. It logs "not implemented", pops the
  arguments, and returns without touching either inventory.

## Divergence

A script call `Npc_GiveItem(giver, item, receiver)` in the original removes one unit of `item` from
`giver` and adds it to `receiver` (closing any trade window showing that item). In OpenGothic the
call is routed to the default not-implemented handler, so **no item is transferred** — the giver
keeps the item and the receiver gets nothing. Any mod (or content) that relies on this external to
hand an item from one NPC to another is silently broken.

## Proposed patch

The transfer primitive already exists and is exercised by the trade path:
`Npc::addItem(size_t id, Npc& from, size_t count)` (grep-verified at `game/world/objects/npc.h:354`,
impl `game/world/objects/npc.cpp:3631`), which calls `Inventory::transfer(...)` to move `count` units
from `from` to the target (unequipping the giver's slot if a full equipped stack is moved). Bind the
external to reuse it with the hard-coded count of 1.

`game/game/gamescript.h` — add the declaration next to the other `npc_*` item externals (~line 302):

```cpp
// OLD
    int  npc_removeinvitems  (std::shared_ptr<zenkit::INpc> npcRef, int itemId, int amount);
// NEW
    int  npc_removeinvitems  (std::shared_ptr<zenkit::INpc> npcRef, int itemId, int amount);
    void npc_giveitem        (std::shared_ptr<zenkit::INpc> giverRef, int itemInstance, std::shared_ptr<zenkit::INpc> receiverRef);
```

`game/game/gamescript.cpp` — register the binding in the `bindExternal` block (~line 178):

```cpp
// OLD
  bindExternal("npc_removeinvitems",             &GameScript::npc_removeinvitems);
// NEW
  bindExternal("npc_removeinvitems",             &GameScript::npc_removeinvitems);
  bindExternal("npc_giveitem",                   &GameScript::npc_giveitem);
```

`game/game/gamescript.cpp` — add the implementation (e.g. after `npc_removeinvitems`, ~line 2318):

```cpp
// NEW
void GameScript::npc_giveitem(std::shared_ptr<zenkit::INpc> giverRef, int itemInstance,
                              std::shared_ptr<zenkit::INpc> receiverRef) {
  // NOTE: in original-game Npc_GiveItem @0x006e73e0 removes exactly one unit of the item from the
  // giver (RemoveFromInv @0x007495f0 with a hard-coded count of 1) and deposits it into the receiver
  // (PutInInv @0x00749350). Declared arg order is (giver, item, receiver); the engine pops the
  // receiver first and the giver last. OpenGothic left this external unbound, so the call hit the
  // default not-implemented handler and transferred nothing.
  auto giver    = findNpc(giverRef);
  auto receiver = findNpc(receiverRef);
  if(giver==nullptr || receiver==nullptr || itemInstance<0)
    return;
  if(giver->itemCount(uint32_t(itemInstance))==0)
    return;
  receiver->addItem(uint32_t(itemInstance),*giver,1);
  }
```

Not reproduced by this patch (intentionally out of scope, none affects the core ownership/count
transfer): the trade-container dismissal of step 2, and the giver's equipped-flag re-equip of step 3
(`Inventory::transfer` already unequips the giver when a full equipped stack moves). If exact fidelity
is required these can be added later, but they are secondary to the missing transfer itself.

DEFERRED alternative: if maintainers consider an unbound-but-vanilla-unused external out of scope,
this can be closed as "mod-only"; the divergence is nonetheless real and the fix above is surgical
and build-verifiable.
