# Wld_DetectItem: flag mask tested against main_flag only (original ORs main_flag|flags)

**Confidence:** High

## Original function + address
`oCNpc::DetectItem` (Gothic2.exe `0x0073fd40`), reached from the `Wld_DetectItem`
external thunk (`0x006e0e40`). For each candidate item the original computes the
bitwise OR of the item's two adjacent flag ints (the item's category field and its
type-flags field) and tests the script-supplied mask against that union: it accepts an
item when `mask & (categoryFlags | typeFlags)` is non-zero. It additionally rejects any
item whose flags carry bit `0x800000` (the "do not detect / no-focus" marker) before
considering distance, and then keeps the nearest accepted item by squared distance.

In the canonical Daedalus `C_ITEM`, the category bits (ITEM_KAT_NF / FF / MUN / ...)
live in `mainflag`, while weapon/wear subtype bits (ITEM_SWD, ITEM_AXE, ITEM_TORCH,
ITEM_RING, ITEM_AMULET, ...) live in `flags`. Because the original ORs both fields, a
script call such as `Wld_DetectItem(ITM_SWD)` or `Wld_DetectItem(ITM_TORCH)` matches
items whose subtype bit is in `flags`.

## OpenGothic location
`game/game/gamescript.cpp:1839-1847` (`wld_detectitem`):
the lambda tests `(it.handle().main_flag & flags) == 0` and skips on no-match — it never
consults the item's `flags` field, and it has no `0x800000` exclusion.

## Divergence
- A `Wld_DetectItem` call whose mask is a subtype bit (ITM_SWD, ITM_AXE, ITM_BOW,
  ITM_TORCH, ITM_RING, ITM_AMULET, ITM_2HD_*, ...) lives in the item's `flags` field, so
  in OpenGothic it can never match and the external returns false, whereas the original
  detects the item. Category masks (ITM_CAT_*) still work because those live in
  `main_flag`.
- Items explicitly flagged no-detect (bit `0x800000`) are detectable in OpenGothic but
  excluded in the original.

## Proposed patch
```cpp
// game/game/gamescript.cpp  (wld_detectitem lambda, ~line 1839)
// OLD
  world().detectItem(npc->position(), float(npc->handle().senses_range), [npc,&ret,&dist,flags](Item& it) {
    if((it.handle().main_flag&flags)==0)
      return;
    float d = (npc->position()-it.position()).quadLength();

// NEW
  world().detectItem(npc->position(), float(npc->handle().senses_range), [npc,&ret,&dist,flags](Item& it) {
    // NOTE: in original-game oCNpc::DetectItem tests the mask against (main_flag | flags),
    // i.e. category bits AND subtype bits, and skips items carrying the 0x800000 no-detect flag.
    const uint32_t itFlags = uint32_t(it.handle().main_flag) | uint32_t(it.handle().flags);
    if((uint32_t(it.handle().flags) & 0x800000u)!=0)
      return;
    if((itFlags & uint32_t(flags))==0)
      return;
    float d = (npc->position()-it.position()).quadLength();
```
