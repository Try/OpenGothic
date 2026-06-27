# Consumable parity: eating FOOD does not restore HP by its `nutrition` value

**Confidence:** High (on the divergence; Medium on the exact placement of the fix — see Notes).

## Original function + address

`oCNpc::UseItem` (Gothic2.exe `0x0073bc10`).

After the `CanUse` gate (`0x007319b0`, which only aborts for the player), the very first
thing `UseItem` does is test the item's runtime flag word with `oCItem::HasFlag(0x20)`
(`oCItem::HasFlag` @ `0x007126d0` returns `(item[0x158] & arg) == arg`). The bit `0x20`
is the FOOD category bit: `oCItem::InitByScript` (`0x00711bd0`) folds the script's
`mainflag` into the runtime flag word right after instance creation
(`item[0x158] |= item[0x154]`, i.e. `flags |= mainflag`), and `mainflag` bit 5 (`0x20`)
is the FOOD category. So `HasFlag(0x20)` is true exactly for FOOD-category items.

When that branch is taken, the engine calls (prose, not pasted):
`ChangeAttribute(this, attrIdx = 0 /*ATR_HITPOINTS*/, val = item[0x1b0])` — i.e.
`oCNpc::ChangeAttribute` (`0x0072ff60`) is invoked to add the item's `nutrition` to the
NPC's current hitpoints, then `AddItemEffects` runs and one unit is removed from the
stack. Field offset `item+0x1b0` is `nutrition`: it sits immediately before
`cond_atr[3]` (`0x1b4`), which precedes `cond_value[3]` (`0x1c0`) and `change_atr[3]`
(`0x1cc`) — the latter two confirmed from the `AddItemEffects` (`0x007320f0`) change_atr
loop that starts at `item+0x1cc`. `ChangeAttribute` then clamps the result up to
`ATR_HITPOINTSMAX`. This is the canonical Gothic behavior: eating an apple / bread /
cheese / stew restores a small amount of HP equal to the food's `nutrition`, driven by
the engine — the food on_state scripts do not themselves call `Npc_ChangeAttribute`.

## OpenGothic file:line

`game/game/inventory.cpp:904` (`Inventory::use`), specifically the generic consumable
fall-through at lines `970`–`1002`. FOOD-category items (`main_flag & ITM_CAT_FOOD`) are
not matched by any equip branch and land here, which runs the cond gate, `setAnimItem`,
and `on_state[0]` — but never reads `nutrition`.

`nutrition` is only ever (de)serialized in OpenGothic — `game/world/objects/item.cpp:52`
and `:122` — and is never applied to any attribute. Result: eating food in OpenGothic
heals nothing (unless a script happens to heal, which the stock G1/G2 food scripts do
not).

## Divergence

Original: consuming a FOOD-category item adds `item.nutrition` to `ATR_HITPOINTS`
(clamped to `ATR_HITPOINTSMAX`) via the engine, independent of any script.
OpenGothic: `nutrition` is dead data; food gives no HP.

## Proposed patch

Grep-verified symbols: `hitem->nutrition` (zenkit `IItem`, `lib/ZenKit/include/zenkit/addon/daedalus.hh:285`),
`Attribute::ATR_HITPOINTS` (`game/game/constants.h:473`),
`Npc::changeAttribute(Attribute,int32_t,bool)` (`game/world/objects/npc.h:217`),
`ITM_CAT_FOOD` (`game/game/constants.h:328`), `mainflag` local already present in
`use()` (`inventory.cpp:910`). `changeAttribute` already performs the HP→HITPOINTSMAX
over-cap clamp (`npc.cpp:1267-1268`), so no separate clamp is needed.

Insert into the consumable fall-through, after the `setAnimItem` success (so it only
fires when the use actually proceeds, mirroring the original's "use happened → apply +
remove one" ordering), gated on the FOOD category:

OLD (`game/game/inventory.cpp`, lines 984-988):
```cpp
  if(!owner.setAnimItem(itData.scheme_name,-1))
    return false;

  // owner.stopDlgAnim();
  setCurrentItem(it->clsId());
```

NEW:
```cpp
  if(!owner.setAnimItem(itData.scheme_name,-1))
    return false;

  // NOTE: in original-game oCNpc::UseItem (Gothic2.exe 0x0073bc10), FOOD-category items
  // (HasFlag 0x20, the mainflag FOOD bit folded into the runtime flags in InitByScript
  // @0x00711bd0) are healed by the engine: ChangeAttribute(ATR_HITPOINTS, nutrition)
  // @0x0072ff60, clamped up to ATR_HITPOINTSMAX. OpenGothic serialized `nutrition` but
  // never applied it, so eating food restored no HP. (changeAttribute already clamps.)
  if(mainflag & ITM_CAT_FOOD)
    owner.changeAttribute(Attribute::ATR_HITPOINTS, itData.nutrition, false);

  // owner.stopDlgAnim();
  setCurrentItem(it->clsId());
```

## Notes / residual risk

- The original handles FOOD in a dedicated early branch that does **not** route through
  the on_state/`setAnimItem` path that OpenGothic uses for all consumables; it applies
  `nutrition` + `AddItemEffects` (on_equip) and removes one unit, with no animation gate.
  This patch instead injects the `nutrition` heal into OpenGothic's existing consume
  path, which is the smallest faithful change but ties the heal to `setAnimItem`
  succeeding. If a stock food item additionally carried `change_atr`/on_equip effects via
  `AddItemEffects`, those remain out of scope here (the equipped `change_atr` path was
  already addressed in `applyArmor`); food's on-consume `change_atr` is a separate
  question and is intentionally **not** bundled into this fix.
- Gate is `mainflag & ITM_CAT_FOOD` (the category field where FOOD lives). The engine's
  `HasFlag` tests `flags | mainflag`; a non-food instance flag occupying bit 5 of `flags`
  is not a real Gothic case, so the category test is the correct intent.
