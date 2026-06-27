# NPC item-use: CanUse failure blocks only the player in the original, but OpenGothic blocks NPCs too

**Confidence:** High

## Original function + address

`oCNpc::UseItem` (Gothic2.exe `0x0073bc10`) and `oCNpc::CanUse` (`0x007319b0`).

`oCNpc::UseItem` begins by calling `CanUse(this,item)`. `CanUse` walks the item's three
`cond_atr[i]` / `cond_value[i]` requirement pairs: a pair is satisfied when `cond_atr[i] < 1`
(empty slot) or when `cond_value[i] <= self.attribute[cond_atr[i]]`. If every pair passes it
returns 1 (usable). If a pair fails, `CanUse` itself invokes the `G_CANNOTUSE` (or
`G_CANNOTCAST` for the magic-circle/`mag_circle` check) parser script — passing the offending
attribute/value — and then returns 0. Crucially, this `G_CANNOTUSE` call happens for **every**
NPC, player or not (the Daedalus script self-gates the on-screen text on its `isPlayer`
parameter).

Back in `UseItem`, the return value is used only here:

> if `CanUse(...) == 0` **and** `this == player`, build the `SC_CANTUSEITEM` conversation
> message, post it, and `return 0` (item not used).

Because the early-out is guarded by `this == player`, an **NPC** whose `CanUse` returned 0 does
**not** return — control falls through to the food/effect branches (`HasFlag(0x20)` →
`AddItemEffects`, etc.) and the NPC consumes/uses the item anyway. In the original engine the
attribute requirement is therefore a hard gate **for the player only**; scripted NPC item use
(`AI_UseItem` → `EV_UseItem` → `UseItem`) ignores unmet `cond_atr` requirements.

## OpenGothic file:line

`game/game/inventory.cpp:950-956` (in `Inventory::use`, the consumable/food/potion/light path,
reached from `Npc::useItem` → `invent.use(...,force=false)` for the `AI_UseItem` queue action at
`game/world/objects/npc.cpp:2710`).

## Divergence

The existing guard blocks **both** players and NPCs whenever the condition check fails (it only
skips when `force==true`, and `force` is false on the `AI_UseItem` NPC path):

```cpp
if(!force) {
  int32_t atr=0,nValue=0;
  if(!it->checkCondUse(owner,atr,nValue)) {
    owner.world().script().printCannotUseError(owner,atr,nValue);
    return false;
    }
  }
```

`checkCondUse` faithfully mirrors the original `cond_atr`/`cond_value` loop, and
`printCannotUseError` faithfully mirrors the `G_CANNOTUSE` invocation (it already forwards
`npc.isPlayer()`). The bug is the unconditional `return false`: it denies an NPC the item, whereas
the original only denies the player. An NPC scripted to `AI_UseItem` a potion/food whose
`cond_atr` requirement exceeds its attributes (e.g. a combat NPC drinking a healing/strength
potion it doesn't "qualify" for) silently does nothing in OpenGothic but succeeds in the original.

The neighboring NOTE (added by a prior fix) is correct that `CanUse` runs for every category, but
it overlooked that the *block* (`return 0`) is `this == player`-gated; the prior fix consequently
over-applied the gate to NPCs.

## Proposed patch

Keep the message call for everyone (matching the original calling `G_CANNOTUSE` unconditionally —
the script self-gates display on `isPlayer`), but only abort the use for the player.

OLD (`game/game/inventory.cpp:946-956`):
```cpp
  // NOTE: in original-game oCNpc::UseItem (Gothic2.exe 0x0073bc10) gates EVERY item category
  // through CanUse, which fails (G_CANNOTUSE) when any cond_value[i] > self.attribute[cond_atr[i]].
  // OpenGothic only gated the equip/setSlot path, so a food/potion with an unmet attribute
  // requirement was consumed anyway.
  if(!force) {
    int32_t atr=0,nValue=0;
    if(!it->checkCondUse(owner,atr,nValue)) {
      owner.world().script().printCannotUseError(owner,atr,nValue);
      return false;
      }
    }
```

NEW:
```cpp
  // NOTE: in original-game oCNpc::UseItem (Gothic2.exe 0x0073bc10) calls CanUse (0x007319b0),
  // which on an unmet cond_atr requirement invokes G_CANNOTUSE for every NPC and returns 0. But
  // UseItem aborts (returns, item not used) ONLY when "this == player"; an NPC whose CanUse fails
  // falls through and uses the item anyway. So the attribute gate blocks the player only.
  if(!force) {
    int32_t atr=0,nValue=0;
    if(!it->checkCondUse(owner,atr,nValue)) {
      owner.world().script().printCannotUseError(owner,atr,nValue);
      if(owner.isPlayer())
        return false;
      }
    }
```

Grep-verified OG symbols: `Item::checkCondUse` (`game/world/objects/item.cpp:358`),
`GameScript::printCannotUseError` (`game/game/gamescript.cpp:988`, already forwards
`npc.isPlayer()`), `Npc::isPlayer()` (used at `game/game/inventory.cpp:982`,
`game/world/objects/npc.cpp:195`). Build-safe: single added `if(owner.isPlayer())` guard around the
existing early return.
