# Item-use: consumable (food/potion) skips the cond_atr requirement gate

**Confidence:** Medium

## Original function + address
`oCNpc::UseItem(oCItem*)` (Gothic2.exe 0x0073bc10). The very first thing it does is
call `oCNpc::CanUse(item)` (0x007319b0). When `CanUse` fails and the user is the
player, it posts the `_SC_CANTUSEITEM` conversation message and returns 0 without
ever applying the item or starting the use animation.

`oCNpc::CanUse` (0x007319b0) walks the item's three `cond_atr[i]/cond_value[i]`
pairs (vob offsets 0x1b4/0x1c0). If for any pair `cond_value[i] > self.attribute[cond_atr[i]]`,
it fires the script callback `G_CANNOTUSE(isPlayer, atr, value)` and returns 0. This
gate is item-category-agnostic: it runs for ANY item, including food (ITM_CAT_FOOD,
1<<5) and potions (ITM_CAT_POTION, 1<<7) — the same engine path (`oCNpc::UseItem` ->
`AddItemEffects` 0x007320f0, or the EV_Drink/EV_UseItemToState swallow that calls
`oCNpc::UseItem`) is gated by `CanUse` for every category.

So in the original, a food/potion that carries a `cond_atr` requirement (e.g.
"requires N strength/mana") cannot be consumed by a player who fails it; instead the
G_CANNOTUSE message appears and the item is untouched.

## OpenGothic file:line
`game/game/inventory.cpp:865` `Inventory::use(...)`. The attribute gate
(`checkCondUse` + `printCannotUseError`) exists, but is only invoked on the
equip/`setSlot` path (`inventory.cpp:412`). For the consumable fallthrough
(`inventory.cpp:908`-`932`: torch handling, then `setAnimItem` + `on_state[0]`),
no `checkCondUse`/`printCannotUseError` is called. All equip categories
(weapons/armor/rings/etc.) have already returned above, so by line 919 only
food/potion/docs/light/torch and other non-equip items remain.

## Divergence
A consumable (food/potion) with a non-zero `cond_value` requirement that the player
does not meet:
- Original: blocked, `G_CANNOTUSE` message, item not consumed.
- OpenGothic: consumed anyway — `setAnimItem` + `on_state[0]` run, effect applies,
  count decrements. The requirement is silently ignored.

(Mirrors the existing equip-path gate; primarily affects items/mods that put
`cond_atr` on consumables. Vanilla G2 uses this sparingly, hence Medium not High.)

## Proposed patch
File: `game/game/inventory.cpp`

OLD:
```cpp
  if(!owner.setAnimItem(itData.scheme_name,-1))
    return false;
```
NEW:
```cpp
  // NOTE: in original-game oCNpc::UseItem (Gothic2.exe 0x0073bc10) gates EVERY item
  // category through oCNpc::CanUse (0x007319b0), which fails (and fires G_CANNOTUSE)
  // when any cond_value[i] > self.attribute[cond_atr[i]]. Food/potions with an
  // attribute requirement are therefore blocked for a player who does not meet it;
  // the equip path already enforced this, the consumable path did not.
  if(!force) {
    int32_t atr=0,nValue=0;
    if(!it->checkCondUse(owner,atr,nValue)) {
      owner.world().script().printCannotUseError(owner,atr,nValue);
      return false;
      }
    }

  if(!owner.setAnimItem(itData.scheme_name,-1))
    return false;
```
