# MOBSI use-with-item: non-player NPCs are blocked instead of being auto-supplied the item

**Confidence:** High (root cause confirmed in decompiler; surgical, build-verifiable fix). Distinct
from the already-fixed "conditionFunc/useWithItem all-NPC gate": that fix correctly moved the
*conditionFunc* evaluation out of `if(isPlayer)`; this finding is about the **opposite** error the
same fix introduced for the *useWithItem* half — for non-player NPCs the original never fails the
item gate, it provides the item.

## Original function + address
`oCMobInter::CanInteractWith` (Gothic2.exe @0x00720f40) is the use-with-item gate. After resolving
the required item (`GetUseWithItem()`, vtable slot 0xC8) it branches on `oCNpc::IsAPlayer`
(vtable slot 0x104, @0x007425a0, `this==player`):

- **Player branch:** the item must be in-hand (`oCNpc::HasInHand`, @0x00737360), already the held
  interact-item (npc+0x968), or in the inventory (`oCNpc::IsInInv`, @0x00749160). If none hold, the
  function returns 0 (refused) and — only if the `T_DONTKNOW` gesture is not already playing — sends
  an `oCMsgManipulate(0xD, ...)` shrug. So the player is genuinely gated on possessing the item.
- **Non-player branch:** the engine calls `oCNpc::RemoveFromInv(item,1)`; if the NPC does **not**
  carry the item (`RemoveFromInv` returns null) it **fabricates a fresh instance via the spawn
  manager** (`ogame->GetSpawnManager()->...(0x81,item)`), then `oCNpc::SetInteractItem`. The
  non-player path has **no failure return** on a missing item — a scripted/routine NPC always
  passes the use-with-item gate and is simply handed the item for the animation.

`oCMobInter::CanChangeState` (@0x0071fc40) shows the same `IsAPlayer`-gated structure (the
inventory-removal block is entered only when `IsAPlayer` is true), confirming the item requirement
is a player-only refusal.

## OpenGothic file:line
`game/world/objects/interactive.cpp:742-749` in `Interactive::checkUseConditions`:

```cpp
if(!useWithItem.empty()) {
    size_t it = sc.findSymbolIndex(useWithItem);
    if(it!=size_t(-1) && npc.itemCount(it)==0) {
      if(isPlayer)
        sc.printMobMissingItem(npc);
      return false;
      }
    }
```

## Divergence
This block returns `false` for **any** NPC whose `itemCount(useWithItem)==0`, not just the player.
Per `CanInteractWith`, a non-player NPC must never fail this gate — the original auto-supplies the
item (from inventory if present, otherwise spawned) so the use animation proceeds. In OpenGothic a
scripted NPC routed onto an item-gated MOBSI (e.g. a smith working an anvil/forge that declares a
`use_with_item`, or any `ZS_*` routine targeting such a mob) silently fails to attach whenever that
NPC does not happen to carry the item, whereas vanilla lets the NPC use it unconditionally. The
accompanying NOTE at `interactive.cpp:732-735` is also inaccurate for `useWithItem`: it claims the
original "evaluates ... the use-with-item gate for ANY NPC," but the original only *refuses* the
player.

## Proposed patch
Keep `conditionFunc` evaluated for all NPCs (that half of the prior fix is correct), but make only
the **useWithItem inventory-failure player-only**, matching the original's player-only refusal.
OpenGothic does not model the held interact-item / item fabrication, so the faithful behavioral
equivalent for a non-player NPC is "do not block."

OLD (`game/world/objects/interactive.cpp:742-749`):
```cpp
  if(!useWithItem.empty()) {
    size_t it = sc.findSymbolIndex(useWithItem);
    if(it!=size_t(-1) && npc.itemCount(it)==0) {
      if(isPlayer)
        sc.printMobMissingItem(npc);
      return false;
      }
    }
  return true;
```

NEW:
```cpp
  // NOTE: in original-game oCMobInter::CanInteractWith (Gothic2.exe @0x00720f40) the use-with-item
  // gate refuses ONLY the player (oCNpc::IsAPlayer @0x007425a0): the player must hold the item in
  // hand, as the interact-item, or in inventory, else it returns 0 with a T_DONTKNOW shrug. A
  // non-player NPC NEVER fails this gate -- the engine removes the item from its inventory, or
  // spawns a fresh instance via the spawn manager, and hands it over as the interact-item. So a
  // routine-driven NPC on an item-gated MOBSI must pass even without carrying the item.
  if(isPlayer && !useWithItem.empty()) {
    size_t it = sc.findSymbolIndex(useWithItem);
    if(it!=size_t(-1) && npc.itemCount(it)==0) {
      sc.printMobMissingItem(npc);
      return false;
      }
    }
  return true;
```

Grep-verified OG symbols: `Interactive::checkUseConditions`, member `useWithItem`/`conditionFunc`,
local `isPlayer` (interactive.cpp:688), `GameScript::findSymbolIndex`, `Npc::itemCount`
(npc.cpp:3796), `Npc::isPlayer`, `GameScript::printMobMissingItem` (gamescript.cpp:1020).
