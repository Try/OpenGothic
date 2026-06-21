# Infinite (ITM_MULTI) munition is consumed on every shot

**Confidence: Medium — DEFER, proposed patch is UNSAFE as written**

> CORRECTION: OG's `ITM_MULTI`=`1<<21` is the *stackable* flag (arrows stack but ARE consumed), NOT the original's infinite-ammo test bit `1<<25`. Guarding `delItem` with `!isMulti()` would make ALL arrows infinite — a regression. The original's endless-munition flag is a distinct bit that OpenGothic may not model; this needs the correct flag identified from Constants.d before any change. Do NOT apply the patch below.

## Original function + address (prose)

- `oCNpc::IsMunitionAvailable` (Gothic2.exe `0x0073c6e0`): the very first thing it does
  after the null check is test the candidate munition item for the daedalus item flag
  with bit value `0x2000000` (the "multi" / endless-munition flag, read out of the
  item's flags field at `oCItem+0x158`). If that flag is set, the function returns
  *available = true* immediately, **without ever looking at the inventory count**.
- `oCNpc::DoInsertMunition` (Gothic2.exe `0x00744190`): when an arrow/bolt is drawn onto
  the bow, the actual inventory decrement (`oCNpcInventory::Remove(munitionId, 1)`,
  `0x0070ce20`) is performed only inside the branch guarded by
  `HasFlag(0x2000000) == 0`. When the flag *is* set, the engine instead spawns a fresh
  projectile item via the item factory and **leaves the inventory stack untouched**.
- Net original behaviour: a munition flagged ITM_MULTI is always reported available and
  is never decremented — it is true infinite ammo.

## OpenGothic file:line

- `game/world/objects/npc.cpp:4133` `Npc::hasAmmunition()` — only checks
  `itemCount(munition) <= 0`; no ITM_MULTI exemption.
- `game/world/objects/npc.cpp:4114` `Npc::shootBow()` — unconditionally calls
  `invent.delItem(munition, 1, *this)`.
- `game/game/inventory.cpp:286` `Inventory::delItem()` — decrements the count for any
  item; no `isMulti()` guard.

## Divergence

In OpenGothic an ITM_MULTI munition is decremented one unit per shot exactly like an
ordinary stack, so it eventually runs out. In the original such a munition is never
consumed. Any NPC/quest set up with an endless-ammo munition (e.g. tutorial/cheat or
scripted archers given a multi-munition) loses ammo in OG where the original keeps it.

Note the flag-bit discrepancy: the original engine reads bit `0x2000000` (`1<<25`) out
of the item flags, while OpenGothic's `ITM_MULTI` constant
(`game/game/constants.h:336`) is `1<<21` and `Item::isMulti()` tests that bit. The
behavioural gap (multi munition consumed) is unambiguous; the safe surgical fix below
keys the guard off `Item::isMulti()` and also documents the bit-value question so it can
be reconciled against `Constants.d`.

## Proposed patch

File: `game/world/objects/npc.cpp`

OLD:
```cpp
  auto& b = owner.shootBullet(*itm,*this,currentTarget,focOverride);

  invent.delItem(size_t(munition),1,*this);
```

NEW:
```cpp
  auto& b = owner.shootBullet(*itm,*this,currentTarget,focOverride);

  // NOTE: in original-game oCNpc::DoInsertMunition (0x00744190) the inventory decrement
  // is skipped when the munition carries the ITM_MULTI ("endless munition") flag, and
  // oCNpc::IsMunitionAvailable (0x0073c6e0) reports such a munition as always available.
  // ITM_MULTI munition must therefore never be consumed.
  if(!itm->isMulti())
    invent.delItem(size_t(munition),1,*this);
```

Optional hardening (so a 0-count never blocks an ITM_MULTI munition), file
`game/world/objects/npc.cpp`, `Npc::hasAmmunition()`:

OLD:
```cpp
  const int32_t munition = active->handle().munition;
  if(munition<0 || invent.itemCount(size_t(munition))<=0)
    return false;
  return true;
```

NEW:
```cpp
  const int32_t munition = active->handle().munition;
  if(munition<0)
    return false;
  // NOTE: in original-game oCNpc::IsMunitionAvailable (0x0073c6e0) an ITM_MULTI munition
  // is always available regardless of the carried count.
  if(auto itm = invent.getItem(size_t(munition)))
    if(itm->isMulti())
      return true;
  return invent.itemCount(size_t(munition))>0;
```
