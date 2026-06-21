# Lock/Mobsi: NPC interactions skip conditionFunc / useWithItem gate

**Confidence:** Medium

## Original function

`oCMobInter::CanInteractWith` (Gothic2.exe `0x00720f40`). This base-class gate runs
for *any* `oCNpc`, player or not. After the ideal-position / use-with-item handling
it reaches the condition-function block: if the mob has a condition function
(`onState`/condition slot non-empty) it sets the parser `self`/`other` instances and
calls the Daedalus condition function. If that function returns 0 the routine returns
0 (interaction denied). There is **no player-only guard** around this call — the only
player checks (`oCNpc::IsAPlayer`, vtable `+0x104`) gate the *HUD/message* side-effects
(the "T_DONTKNOW" conversation, manipulate messages), not the condition evaluation
itself. So a non-player NPC trying to use a mobsi is blocked when the mob's
condition function returns 0, and equally must satisfy the use-with-item requirement.

## OpenGothic

`game/world/objects/interactive.cpp:671` `Interactive::checkUseConditions`, called from
`Interactive::attach` (`interactive.cpp:810`) for both player and NPC (`Npc::setInteraction`
-> `attach`).

The entire conditionFunc + useWithItem block (lines 706-718) is wrapped inside
`if(isPlayer)` (opened at line 676). Consequently a non-player NPC never evaluates
`conditionFunc` nor the `useWithItem` requirement, and always passes those checks.

## Divergence

In the original the mob condition function (and use-with-item) gates **every** NPC.
In OpenGothic it gates **only the player**; routine/script-driven NPCs use mobsis even
when the condition function would deny them. Concrete branch difference: the
condition-function `CallFunc ... if(result==0) return 0` runs unconditionally in
`CanInteractWith`, but is behind `if(isPlayer)` in OpenGothic.

## Proposed patch

Evaluate conditionFunc / useWithItem for all NPCs; keep the key/lockpick HUD
messages player-only (those already early-return inside the `isPlayer` block).

```cpp
// OLD (interactive.cpp, inside checkUseConditions)
    if(!conditionFunc.empty()) {
      const int check = sc.invokeCond(npc,conditionFunc);
      if(check==0)
        return false;
      }

    if(!useWithItem.empty()) {
      size_t it = sc.findSymbolIndex(useWithItem);
      if(it!=size_t(-1) && npc.itemCount(it)==0) {
        sc.printMobMissingItem(npc);
        return false;
        }
      }
    }
  return true;
  }
```

```cpp
// NEW
    }

  // NOTE: in original-game oCMobInter::CanInteractWith (Gothic2.exe 0x00720f40)
  // the condition-function and use-with-item gates are evaluated for ANY oCNpc,
  // not just the player; only the HUD messages are player-only.
  if(!conditionFunc.empty()) {
    const int check = sc.invokeCond(npc,conditionFunc);
    if(check==0)
      return false;
    }

  if(!useWithItem.empty()) {
    size_t it = sc.findSymbolIndex(useWithItem);
    if(it!=size_t(-1) && npc.itemCount(it)==0) {
      if(isPlayer)
        sc.printMobMissingItem(npc);
      return false;
      }
    }
  return true;
  }
```

(The closing brace of the `if(isPlayer)` block is moved up so the key/lockpick
messaging stays player-only while conditionFunc/useWithItem apply to all NPCs.)
