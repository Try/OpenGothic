# Trade buy/sell charges gold for the requested count, not the count actually transferred

**Confidence:** Medium

## Original function + address

`oCViewDialogTrade::OnTransferRight(short)` at `0x0068bb40` (player buys from
merchant) and `oCViewDialogTrade::OnTransferLeft(short)` at `0x0068b840` (player
sells to merchant), both in
`P:\dev\g2addon\release\Gothic\_roman\oViewDialogTrade.cpp`.

Both handlers move the requested amount **one unit at a time** inside a loop.
Each iteration calls `RemoveSelectedItem()` to pull a single unit out of the
source container, and *only if that call returns a non-null item* does it then
move gold for that unit:

- Buy (`OnTransferRight`): per unit, `iVal = oCItem::GetValue(unit)`; if the
  player can afford `iVal` it removes `iVal` gold (`RemoveCurrencyItem`) and
  inserts the unit into the player inventory; otherwise it puts the unit back and
  shows `PLAYER_TRADE_NOT_ENOUGH_GOLD`.
- Sell (`OnTransferLeft`): per unit, `iVal = ROUND(GetValueMultiplier * GetValue(unit))`
  (min 1) and a currency item of that worth is created for the player.

The decisive property: gold is moved **inside the same per-unit iteration that
actually removed an item**. The moment `RemoveSelectedItem()` returns null
(source stack exhausted), the loop body does nothing and no further gold changes
hands. Gold transferred is therefore always exactly `price * (units actually
moved)`.

## OpenGothic location

`game/world/objects/npc.cpp:3488-3513` (`Npc::sellItem` / `Npc::buyItem`), which
delegate the item move to `Inventory::transfer`
(`game/game/inventory.cpp:317-344`).

```cpp
void Npc::sellItem(size_t id, Npc &to, size_t count) {
  if(id==owner.script().goldId()->index())
    return;
  int32_t price = invent.sellPriceOf(id);
  Inventory::transfer(to.invent,invent,this,id,count,owner);
  invent.addItem(owner.script().goldId()->index(),size_t(price)*count,owner);
  }

void Npc::buyItem(size_t id, Npc &from, size_t count) {
  ...
  Inventory::transfer(invent,from.invent,nullptr,id,count,owner);
  if(price>=0)
    invent.delItem(owner.script().goldId()->index(),size_t( price)*count,*this); else
    invent.addItem(owner.script().goldId()->index(),size_t(-price)*count,owner);
  }
```

## Divergence

`Inventory::transfer` clamps the move to what the source actually holds
(`inventory.cpp:326-327`: `if(count>it.count()) count=it.count();`), but `count`
is passed **by value**, so the clamp never propagates back to `buyItem`/`sellItem`.
Those callers then compute the gold delta from the *original, unclamped* `count`.

Consequence: if the requested `count` exceeds the source stock, OpenGothic moves
only the available units yet still bills/pays `price * requestedCount` gold. The
original engine, moving and paying per unit, can never charge for a unit it did
not move. So:

- Buy with `count` > merchant stock: player loses gold for items never received.
- Sell with `count` > player stock: player is paid for items never delivered.

Reachability: today the only callers are the trade UI (`inventorymenu.cpp:525,527`),
which clamps `itemCount` to the selected stack (`inventorymenu.cpp:512-514`), so
the bug is currently dormant through the menu. It is a latent gold-accounting
divergence in the public `Npc::buyItem`/`Npc::sellItem` API: any future or
script-driven caller (or a UI lootMode/quantity tweak) that passes a count larger
than stock desynchronizes gold from items. Kept at Medium for that reason.

## Proposed patch

Make `transfer` report the number of units it actually moved, and have the trade
callers settle gold against that. `transfer` is `static void` at
`inventory.h:67`; widen it to return `size_t`.

```cpp
// game/game/inventory.h
```
OLD:
```cpp
    static void  transfer(Inventory& to, Inventory& from, Npc *fromNpc, size_t cls, size_t count, World &wrld);
```
NEW:
```cpp
    static size_t transfer(Inventory& to, Inventory& from, Npc *fromNpc, size_t cls, size_t count, World &wrld);
```

```cpp
// game/game/inventory.cpp
```
OLD:
```cpp
void Inventory::transfer(Inventory &to, Inventory &from, Npc* fromNpc, size_t itemSymbol, size_t count, World &wrld) {
  for(size_t i=0;i<from.items.size();++i){
    auto& it = *from.items[i];
    if(it.clsId()!=itemSymbol)
      continue;

    from.sorted = false;
    to.sorted   = false;

    if(count>it.count())
      count=it.count();

    if(it.count()==count) {
      if(it.isEquipped()) {
        if(fromNpc==nullptr){
          Log::e("Inventory: invalid transfer call");
          return; // error
          }
        from.unequip(&it,*fromNpc);
        }
      to.addItem(std::move(from.items[i]));
      from.items.erase(from.items.begin()+int(i));
      } else {
      it.setCount(it.count()-count);
      to.addItem(itemSymbol,count,wrld);
      }
    }
  }
```
NEW:
```cpp
size_t Inventory::transfer(Inventory &to, Inventory &from, Npc* fromNpc, size_t itemSymbol, size_t count, World &wrld) {
  // NOTE: in original-game oCViewDialogTrade::OnTransferRight/OnTransferLeft
  // (Gothic2.exe 0x0068bb40 / 0x0068b840) move trade items one unit at a time and
  // settle gold only for units actually removed; a request beyond stock simply
  // stops. Return the count truly moved so gold accounting can never bill/pay for
  // items that were not transferred.
  for(size_t i=0;i<from.items.size();++i){
    auto& it = *from.items[i];
    if(it.clsId()!=itemSymbol)
      continue;

    from.sorted = false;
    to.sorted   = false;

    if(count>it.count())
      count=it.count();

    if(it.count()==count) {
      if(it.isEquipped()) {
        if(fromNpc==nullptr){
          Log::e("Inventory: invalid transfer call");
          return 0; // error
          }
        from.unequip(&it,*fromNpc);
        }
      to.addItem(std::move(from.items[i]));
      from.items.erase(from.items.begin()+int(i));
      } else {
      it.setCount(it.count()-count);
      to.addItem(itemSymbol,count,wrld);
      }
    return count;
    }
  return 0;
  }
```

```cpp
// game/world/objects/npc.cpp
```
OLD:
```cpp
  int32_t price = invent.sellPriceOf(id);
  Inventory::transfer(to.invent,invent,this,id,count,owner);
  invent.addItem(owner.script().goldId()->index(),size_t(price)*count,owner);
```
NEW:
```cpp
  int32_t price = invent.sellPriceOf(id);
  count = Inventory::transfer(to.invent,invent,this,id,count,owner);
  invent.addItem(owner.script().goldId()->index(),size_t(price)*count,owner);
```
OLD:
```cpp
  Inventory::transfer(invent,from.invent,nullptr,id,count,owner);
  if(price>=0)
    invent.delItem(owner.script().goldId()->index(),size_t( price)*count,*this); else
    invent.addItem(owner.script().goldId()->index(),size_t(-price)*count,owner);
```
NEW:
```cpp
  count = Inventory::transfer(invent,from.invent,nullptr,id,count,owner);
  if(price>=0)
    invent.delItem(owner.script().goldId()->index(),size_t( price)*count,*this); else
    invent.addItem(owner.script().goldId()->index(),size_t(-price)*count,owner);
```

`grep "Inventory::transfer" game/` shows five call sites in `npc.cpp`
(lines 3477/3481/3485 are chest/give moves in `moveItem`/`addItem` that do not
touch gold; 3492/3509 are the sell/buy sites patched above). Widening the return
type from `void` to `size_t` is safe: the three non-trade callers simply discard
the result (legal in C++), and only the two trade callers are updated to consume
it.
