# Active spell-slot 10 (8th magic key) is dropped on save-load

**Confidence:** High. Pure OpenGothic-internal serialization asymmetry: the save path
writes a magic-slot id in the range `[3,10]`, but the load path only accepts `[3,9]`,
so the highest magic slot is silently lost. Verified by reading both halves of the
serializer; no other code path re-derives `active` on load.

## Original function + address (prose)
In the original the player's number keys 3,4,5,6,7,8,9,0 map to **eight** magic-circle
keys. `oCMag_Book::GetNoOfSpellByKey` @0x00479ce0 and `oCMag_Book::GetSpellByKey`
@0x00479c60 select the front spell by that 1-based key, and the book's per-key
occupancy mask (`oCMag_Book+0x78`, tested as `1 << (key-1)`) spans the full set of
magic keys. The original's NPC/inventory persistence (oCNpc archive of its
`oCInventory_Marion`) restores the currently-readied magic slot for *all* of those
keys, including the last one — there is no key-range truncation on restore. OpenGothic
models the same eight keys as `numslot[0..7]` (keys 3..10), so a faithful save/load
must round-trip the full `id` range `[3,10]`.

## OpenGothic file:line
`/Users/admin/Downloads/opengothic/game/game/inventory.cpp:144`
(`Inventory::implLoad`, the `active` restore), against the matching save block at
`/Users/admin/Downloads/opengothic/game/game/inventory.cpp:195-197`.

## Divergence
Save side (lines 190-198) encodes the active pointer:

```cpp
for(int i=0;i<8;++i)
  if(active==&numslot[i])
    id = uint8_t(3+i);     // i==7 -> id==10
```

so `id` ranges over `[3,10]` for the eight magic slots. Load side (lines 138-145):

```cpp
if(id==1)            active=&melee;
else if(id==2)       active=&range;
else if(3<=id && id<10) active=&numslot[id-3];   // accepts only 3..9
```

The upper bound is `id<10`, which **excludes `id==10`**. If the player saves while the
8th magic slot (key 10, `numslot[7]`) is the readied spell, on reload none of the
branches match and `active` stays at its default `nullptr` (inventory.h:169). The
readied-spell state is therefore lost: `currentSpellSlot()` returns `Item::NSLOT`,
`switchActiveWeapon`/rune-view and weapon-stat bookkeeping no longer know which spell is
active, and the mana-bar/spell-selection state for that slot is inconsistent after load.
The bug is invisible for melee (`id==1`), ranged (`id==2`), and the first seven magic
slots (`id` 3..9); only the highest magic key regresses.

## Proposed patch
OLD (`inventory.cpp:144`):
```cpp
  else if(3<=id && id<10)
    active=&numslot[id-3];
```
NEW:
```cpp
  // NOTE: in original-game the player has eight magic keys (3..0), mirrored here as
  // numslot[0..7] (ids 3..10); the save block above writes id up to 10, so the load
  // bound must accept id==10 (8th magic slot). `id<10` dropped the highest readied
  // spell on reload, leaving Inventory::active==nullptr.
  else if(3<=id && id<11)
    active=&numslot[id-3];
```

Grep-verified symbols: `Inventory::numslot[8]` (inventory.h:173), `Inventory::active`
default `nullptr` (inventory.h:169), `Item::NSLOT` / `currentSpellSlot()`
(inventory.cpp:717-722). One-line bound change, build-verifiable, symmetric with the
existing save loop `for(int i=0;i<8;++i)`.
