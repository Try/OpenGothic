# Npc_KnowsInfo ignores the permanent flag (permanent infos wrongly report "known")

**Confidence:** Medium-high

## Original function + address

- `Npc_KnowsInfo` external handler lives in the Ulfi external-definition block
  (`FUN_006dc4a0`, the `Npc_KnowsInfo` case around `0x006dc785`). It pops the info-instance
  symbol index, calls `oCInfoManager::InformationTold` @ `0x007031a0`, and pushes that as the
  parser return value. The NPC argument is unused (already noted in OG).
- `oCInfoManager::InformationTold(int)` @ `0x007031a0` walks the info list, finds the `oCInfo`
  whose instance-symbol field (`+0x50`) equals the requested index, then returns:
  `if (permanent /*+0x48*/ != 0) return 0; else return told /*+0x4c*/;`
- This is the same masking used by `oCInfo::WasTold` @ `0x00703900`
  (`if (+0x48 /*permanent*/ != 0) return 0; return +0x4c /*told*/;`).

Net effect in the original: a **permanent** C_Info is *never* reported as told/known, even after
it has been played and its internal told flag (`oCInfo::SetTold` @ `0x00703910`) was set. The
permanent flag masks the told flag at every read site.

## OpenGothic file:line

`game/game/gamescript.cpp:2159` — `GameScript::npc_knowsinfo`.

```cpp
bool GameScript::npc_knowsinfo(std::shared_ptr<zenkit::INpc> npcRef, int infoinstance) {
  // NOTE (existing): query against hero, npc arg ignored.
  auto pl  = world().player();
  auto npc = (pl!=nullptr) ? pl : findNpc(npcRef);
  if(npc==nullptr)
    return false;
  return doesNpcKnowInfo(npc->handle(), uint32_t(infoinstance));
  }
```

## Divergence

`doesNpcKnowInfo` returns mere membership in the told set (`dlgKnownInfos`), with no regard for
the `permanent` flag. `GameScript::exec` (`gamescript.cpp:970-971`) calls `setNpcInfoKnown`
for the chosen main info **unconditionally**, including permanent infos. Therefore, after a
permanent info has been played once, `Npc_KnowsInfo(..., THAT_INFO)` returns `true` in
OpenGothic, whereas the original `InformationTold` masks permanent infos and returns `false`
(0) forever.

This diverges script logic for any C_Info declared `permanent` that is also queried via
`Npc_KnowsInfo` (e.g. a script gating a branch on whether a repeatable info was ever shown).
The dialog-availability gate itself is unaffected (`dialogChoices` line 895 already special-cases
`!info.permanent`); only the script-visible `Npc_KnowsInfo` query is wrong.

## Proposed patch

Mask the told result with the info's `permanent` flag, mirroring `InformationTold`. All symbols
are grep-verified: `dialogsInfo` (`gamescript.h:470`, `std::vector<std::shared_ptr<zenkit::IInfo>>`),
`info->symbol_index()` (used at `gamescript.cpp:2648`), `info->permanent` (used at `:2649`),
`doesNpcKnowInfo`.

OLD:
```cpp
  auto pl  = world().player();
  auto npc = (pl!=nullptr) ? pl : findNpc(npcRef);
  if(npc==nullptr)
    return false;
  return doesNpcKnowInfo(npc->handle(), uint32_t(infoinstance));
  }
```

NEW:
```cpp
  auto pl  = world().player();
  auto npc = (pl!=nullptr) ? pl : findNpc(npcRef);
  if(npc==nullptr)
    return false;
  if(!doesNpcKnowInfo(npc->handle(), uint32_t(infoinstance)))
    return false;
  // NOTE: in original-game oCInfoManager::InformationTold @0x007031a0 (and oCInfo::WasTold
  // @0x00703900) mask the told flag with `permanent`: a permanent C_Info is never reported as
  // known, even after it was played and SetTold @0x00703910 ran. OpenGothic stored permanent
  // infos in the told set (exec() calls setNpcInfoKnown unconditionally), so Npc_KnowsInfo wrongly
  // returned true for them.
  for(auto& info : dialogsInfo)
    if(info->symbol_index()==uint32_t(infoinstance))
      return !info->permanent;
  return true;
  }
```
