# createinvitem/createinvitems engine paths vs oCNpc::CreateItems

**Confidence:** NO FINDING (common paths faithful; only obscure edge-cases diverge, and the
one structural divergence overlaps the already-deferred `craft-createitem-inventory-pollution`)

## Original functions + addresses (prose)
- `Npc_CreateInvItem` external = `FUN_006e7620`. Creates exactly one item vob via
  `oCWorld::CreateVob(0x81, instance)`, `oCNpc::PutInInv`, then `SetInstance("ITEM", item)`.
- `Npc_CreateInvItems` external = `FUN_006e7b60`. Reads (item, amount), calls
  `oCNpc::CreateItems(npc, item, amount)`, then restores the parser SELF/OTHER/VICTIM
  instances. There is **no** `amount > 0` guard around the `CreateItems` call (the only
  precondition is `npc != null && ogame != null && *(ogame+0x28)==0`).
- `oCNpc::CreateItems` @ `0x00730800`. Branches on `oCItem::MultiSlot()`:
  - multi-slot (gold/arrows/food/potions/runes): create ONE vob, write amount field
    (`oCItem+0x32c`) = N, `PutInInv` once. ITEM bound to the merged stack.
  - non-multi-slot (unique weapons/armour): create the first vob + `PutInInv`
    **unconditionally**, then if `amount>1` loop `amount-1` more times creating that many
    additional distinct `oCItem` objects (item instance constructor runs N times). ITEM
    bound to the last created object.

## OpenGothic file:line
- `game/game/gamescript.cpp:3684` (`createinvitem`) and `:3692` (`createinvitems`).
- `game/game/inventory.cpp:267` (`Inventory::addItem(size_t,count,owner)`): always one
  stacked `Item` with `setCount(count)`, irrespective of the item's MULTI flag; guards
  `count<=0 -> nullptr`. `createinvitems` additionally guards `amount>0`.
- `GameScript::storeItem` (`game/game/gamescript.cpp:841`) correctly mirrors the original
  `SetInstance("ITEM", ...)` binding.

## Divergence analysis
1. Common path (multi-slot item, positive amount — gold/arrows/food/potions, i.e. >99% of
   real `CreateInvItems` calls, and all smithing/cooking/alchemy `CreateInvItem(s)` results):
   original mints/merges one stack of count N; OpenGothic does the same. **Faithful.**
   `ITEM` global binding (`storeItem`) is faithful. `createinvitem` (always count 1) is
   faithful.

2. Non-multi-slot item with amount>1: original creates N separate object instances
   (constructor side-effects, if any, fire N times) whereas OpenGothic keeps a single
   stacked `Item` (constructor once). This is a real divergence, but (a) it is the
   structural "create N real vobs" behaviour already captured/deferred under
   `craft-createitem-inventory-pollution`, (b) OpenGothic's entire inventory model is
   single-Item-with-count by design, so any "fix" would be invasive, not surgical, and
   (c) shipped G2 scripts effectively never `CreateInvItems` a non-stackable weapon/armour
   with amount>1.

3. amount <= 0: original (no guard) still mints at least one object (non-multi) or a
   count-0/negative ghost stack (multi); OpenGothic mints nothing. Genuine but obscure
   script-bug edge with no known shipped trigger and undesirable original behaviour to
   replicate. Not high-confidence/surgical.

No high-confidence, surgical, build-verifiable, non-excluded engine divergence found in the
item-creation/smithing/repair give-item hooks. **NO FINDING.**
