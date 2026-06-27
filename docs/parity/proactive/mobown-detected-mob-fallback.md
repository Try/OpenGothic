# Npc_IsDetectedMobOwnedByNpc drops the obstacle-mob (RbtObstacleVob) fallback

**Confidence:** High

## Original function + address
`Npc_IsDetectedMobOwnedByNpc` external handler at `Gothic2.exe 0x006ed540`
(registered via `DefineExternals_Ulfi` / `oGameExternal.cpp`). After popping the two
`C_NPC` arguments (user = first declared arg, owner = last declared arg), the original
resolves the candidate mob in two stages:

1. `oCNpc::GetInteractMob(user)` — the mob the user NPC is actively attached to.
2. If that is null, it falls back to `oCNpc::GetRbtObstacleVob(user)` and dynamic-casts
   that vob to `oCMobInter`. This is the "robust obstacle vob" — the interactive object
   the NPC most recently *collided with / bumped into* during movement.

It then returns `mob->IsOwnedByNpc(owner->GetInstance())` (vtable +0x84;
`oCMOB::IsOwnedByNpc` @0x0071c1b0 compares the mob's owner-instance index at +0x174).

The sibling external `Npc_IsDetectedMobOwnedByGuild` @0x006ed750 uses the **identical
two-stage resolution** (GetInteractMob, then RbtObstacleVob fallback) before calling
`oCMOB::IsOwnedByGuild` (vtable +0x80, @0x0071c190).

## OpenGothic file:line
`game/game/gamescript.cpp:2955-2965` (`GameScript::npc_isdetectedmobownedbynpc`).

## Divergence
OpenGothic resolves the candidate mob with `usr->interactive()` only, which maps to the
original's `GetInteractMob` (currentInteract) — the *attached* mob — and omits the
obstacle-vob fallback. The engine already models that fallback: `Npc::detectedMob()`
(`game/world/objects/npc.cpp:4616`) returns `currentInteract`, else the cached `moveMob`,
and `moveMob` is set by `MoveAlgo::onMoveFailed` (`game/game/movealgo.cpp:939-941`) from
the collided `info.vob` — i.e. exactly the `GetRbtObstacleVob` case — and that same path
fires `PERC_MOVEMOB`. Tellingly, the sibling `npc_isdetectedmobownedbyguild`
(`gamescript.cpp:2977-2978`) already uses `npc->detectedMob()`, so the two siblings are
inconsistent in OG even though the original implements them identically.

Effect: when a script's `PERC_MOVEMOB` / owned-mob theft handler calls
`Npc_IsDetectedMobOwnedByNpc(self, owner)` while the user is merely *colliding with* an
owned bed/chest/bench (not yet attached, so `interactive()==nullptr`), OG returns false
and the ownership/theft gate is silently skipped; the original returns the true ownership
result via the RbtObstacleVob fallback.

## Proposed patch
Grep-verified symbols: `Npc::detectedMob()` (`npc.h:308`, returns `Interactive*`),
`Interactive::ownerName()` (`interactive.h:45`), `Npc::instanceSymbol()` and
`vm.find_symbol_by_index` (both already used in this same function).

```cpp
// OLD
  if(npc!=nullptr && usr!=nullptr && usr->interactive()!=nullptr){
    auto* inst = vm.find_symbol_by_index(npc->instanceSymbol());
    auto  ow   = usr->interactive()->ownerName();
    return inst->name() == ow;
    }

// NEW
  // NOTE: in original-game Npc_IsDetectedMobOwnedByNpc @0x006ed540 the mob is taken from
  // GetInteractMob(user) with a fallback to the collided RbtObstacleVob; detectedMob()
  // models both (currentInteract, else the move-collision moveMob). interactive() alone
  // dropped the obstacle-vob fallback, matching the sibling Npc_IsDetectedMobOwnedByGuild.
  if(npc!=nullptr && usr!=nullptr && usr->detectedMob()!=nullptr){
    auto* inst = vm.find_symbol_by_index(npc->instanceSymbol());
    auto  ow   = usr->detectedMob()->ownerName();
    return inst->name() == ow;
    }
```

(Note: `npc_isdetectedmobownedbyguild` remains a separate, acknowledged stub — it returns
false because OpenGothic never parses/stores the mob `owner_guild`; fixing that requires
guild-name→index resolution and is out of scope here.)
