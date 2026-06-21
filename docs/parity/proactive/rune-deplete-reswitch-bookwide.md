# rune/scroll depletion does not re-select a known spell outside the 8 hotbar slots

**Confidence:** Low (DEFERRED)

## Original function + address

`oCMag_Book::Spell_Cast` (Gothic2.exe @0x004767a0) is the cast-emission path for
the player. After a scroll is fired it decrements the scroll item's charge field
(oCItem +0x32c) and, when that reaches 0, removes the item from the inventory.
At the tail (LAB_00476c63) it then inspects the spell book's spell count
(oCMag_Book +0x8): if the book still holds spells it resets the selected index to
0 and re-sets-up the spell at index 0 (the NPC therefore stays in mage / weapon
mode 7 with a freshly selected spell). Only when the book is empty
(`oCMag_Book +0x8 == 0`) does it post an `oCMsgWeapon(3,...)` — i.e. remove the
drawn weapon and leave magic mode.

Crucially the original spell book (oCMag_Book +0x8 = count, populated by
`oCMag_Book::Register` @0x00475ad0 for every rune/scroll the NPC owns) is an
unbounded list of *all* known spells, not a fixed-size hotbar. So after the last
charge of a scroll is consumed, the original re-selects another *known* spell even
if it was never bound to a number key.

## OpenGothic file:line

`game/world/objects/npc.cpp:3264-3282` (`Npc::commitSpell`)

```
if(active->isSpell()) {
  size_t cnt = active->count();
  invent.delItem(active->clsId(),1,*this);
  if(cnt<=1) {
    Item* spl = nullptr;
    for(uint8_t i=0;i<8;++i) {
      if(auto s = invent.currentSpell(i)) { spl = s; break; }
      }
    if(spl==nullptr) {
      if(spellInfo==0)
        aiPush(AiQueue::aiRemoveWeapon());
      } else {
      drawSpell(spl->spellId());
      }
    }
  }
```

## Divergence

OpenGothic models spells as 8 hotbar `numslot[0..7]` entries (`Inventory`,
verified: `game/game/inventory.h:110 currentSpell(uint8_t)`,
`game/game/inventory.cpp:677 switchActiveSpell`). When a scroll's last charge is
spent, `commitSpell` searches only `numslot[0..7]` for a replacement spell. The
original searches the whole spell book. If the player knows other runes/scrolls
that are not currently assigned to one of the 8 hotbar slots, OpenGothic finds no
replacement and queues `aiRemoveWeapon()` (sheathes / leaves magic mode), whereas
the original would re-select another known spell and stay in mage mode.

## Proposed patch

DEFERRED. Reasons:

1. The behaviors are not 1:1 mappable because the underlying spell-storage models
   differ fundamentally: original = unbounded book (`oCMag_Book::Register` per
   owned magic item); OpenGothic = fixed 8-slot hotbar. There is no
   grep-verified OpenGothic symbol that enumerates "all known spells" in book
   order to mirror the original's index-0 re-selection — `Inventory::items`
   would need a spell filter plus an ordering rule matching
   `oCMag_Book`'s registration order, which is not currently reproduced.
2. The divergence only manifests in the edge case where the player knows more
   spells than are bound to hotbar slots (or has unbound runes) and depletes the
   *last charge of the currently-cast scroll mid-combat — a narrow, hard-to-
   observe condition.
3. A surgical fix risks introducing a new ordering that itself diverges from the
   original's book order, trading one parity gap for another. "Empty beats false
   positives."

Grep-verified symbols referenced: `Npc::commitSpell`, `Item::isSpell`
(`game/world/objects/item.cpp:276`), `Item::count`, `Inventory::delItem`
(`game/game/inventory.cpp:286`), `Inventory::currentSpell`
(`game/game/inventory.h:110`), `Npc::drawSpell` (`game/world/objects/npc.cpp:3816`),
`AiQueue::aiRemoveWeapon`.
