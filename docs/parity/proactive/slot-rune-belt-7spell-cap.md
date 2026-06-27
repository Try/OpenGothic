# Magic-rune belt slots: original caps readied spells at 7, OpenGothic allows 8

**Confidence:** Medium

## Original function + address (prose only)

`oCNpc::Equip` (Gothic2.exe `0x00739c90`) dispatches on `oCNpcInventory::GetCategory`
(`0x0070c690`). Category 3 is the rune/spell branch. When the focused rune is *not* yet
readied (its 0x40000000 "equipped" flag is clear) it tries to register the rune into the
NPC's magic book (`oCMag_Book` at `this+0x914`). Before registering it reads the current
spell count via `oCMag_Book::GetNoOfSpells` (`0x00479b60`, which simply returns the book's
count field) and bails out with `if (6 < count) return;` — i.e. it refuses to ready an 8th
spell, so **at most 7 spells can be readied at once**. The same `if (6 < count) return 0;`
guard also opens `oCMag_Book::Register` (`0x00475bb0`). Re-selecting an already-readied rune
takes the toggle branch (DeRegister + clear flag) and is unaffected by the cap. The cap is
unconditional in `Equip` (there is no force/player distinction on this path, unlike the
weapon/armor `CanUse` gates).

## OpenGothic file:line

- `game/game/inventory.cpp:943` — `Inventory::use`, the `ITM_CAT_RUNE` branch.
- `game/game/inventory.cpp:877` — `Inventory::equipNumSlot`.
- `game/game/inventory.h:173` — `Item* numslot[8]`.
- `game/game/inventory.cpp:144` — load NOTE documenting "eight magic keys (3,4,5,6,7,8,9,0),
  mirrored as numslot[0..7] (ids 3..10)".

## Divergence

OpenGothic models eight fixed spell quick-slots (`numslot[8]`, key bindings 3,4,5,6,7,8,9,0
→ slot ids 3..10). `equipNumSlot` with no slot hint fills the first empty of the eight and
only returns `false` once **all eight** are full, and the explicit key-0 hint binds directly
into `numslot[7]`. There is no count cap. So a player can ready an 8th spell that the
original silently refuses: with 7 spells already readied, pressing key 0 (or otherwise
equipping another rune) succeeds in OpenGothic but is rejected by `oCNpc::Equip`. This is a
behavioral divergence in exactly the "magic-rune belt slots" area. Note the OpenGothic
load-path NOTE describes the 8-key model as a deliberate OG mapping; it does not claim parity
with the original's 7-spell ceiling, which is the source of the discrepancy.

## Proposed patch

Mirror the original's `if (6 < GetNoOfSpells) return;` by refusing to ready a *new* rune once
seven are already readied. Re-binding an already-readied rune to a different key still works
(it does not grow the readied count), matching the original key-reassignment behavior. This
is surgical: it does not change `numslot`'s size, the key map, or the save format, so existing
saves still load; it only blocks the genuinely-8th ready.

OLD (`game/game/inventory.cpp`, `Inventory::use`, rune branch):
```cpp
  if(mainflag & ITM_CAT_RUNE) {
    if(it->isEquipped() && slotHint==it->slot())
      return false;
    if(it->isEquipped())
      unequip(it,owner);
    return equipNumSlot(it,slotHint,owner,force);
    }
```

NEW:
```cpp
  if(mainflag & ITM_CAT_RUNE) {
    if(it->isEquipped() && slotHint==it->slot())
      return false;
    if(it->isEquipped())
      unequip(it,owner);
    // NOTE: in original-game oCNpc::Equip (Gothic2.exe 0x00739c90) the rune branch registers a
    // spell into the magic book only while oCMag_Book::GetNoOfSpells (@0x00479b60) is <= 6 -- the
    // unconditional `if (6 < count) return;` ceiling means at most 7 spells can be readied at once
    // (the same guard opens oCMag_Book::Register @0x00475bb0). OpenGothic exposes eight magic keys
    // (numslot[8], ids 3..10) and let the player ready an 8th spell the original refuses. Cap a
    // newly-readied rune at 7; re-binding an already-readied rune to another key still works since
    // it does not grow the count, mirroring the original's key reassignment.
    if(!it->isEquipped()) {
      uint32_t ready = 0;
      for(auto& s:numslot)
        if(s!=nullptr)
          ++ready;
      if(ready>=7)
        return false;
      }
    return equipNumSlot(it,slotHint,owner,force);
    }
```

Symbols grep-verified: `numslot` (`inventory.h:173`), `Item::isEquipped`, `Item::slot()`
(`item.h:51`), `equipNumSlot` (`inventory.h:138`), `ITM_CAT_RUNE` (`constants.h:332`).

### Residual uncertainty
Medium (not High) because OpenGothic's 8-key/8-slot model is a documented design choice, so
the 8th slot may be an intentional OG quality-of-life extension rather than an oversight; the
fix re-imposes the original 7-spell ceiling for strict parity. If parity policy prefers
preserving OG's 8-key affordance, treat this as DEFERRED (intentional divergence) instead.
