# Npc_KnowsInfo keys on the wrong (passed) NPC instead of a global per-info flag

**Confidence:** Medium

## Original function + address

`Npc_KnowsInfo` external handler is `FUN_006dc4a0` (registered in
`DefineExternals_Ulfi`, handler addr `0x6dc4a0`). The script signature declares
two arguments (`var C_NPC npc, var int infoInstance`), but the handler reads
**only one** parameter — the info instance — with a single `GetParameter` call.
The NPC argument is popped by the VM and **discarded**.

It then resolves the info symbol and calls
`oCInfoManager::InformationTold(infoInstance)`. `InformationTold` walks the info
manager's global info list, matches the `oCInfo` whose instance id (field +0x50)
equals `infoInstance`, and returns its "told" flag (field +0x4c) — gated so that
a permanent info (field +0x48 != 0) always reports 0/false.

Net behavior: the "does the player know this info" answer is a **single global,
per-info flag**, completely independent of which NPC is passed. The told flag is
set once on the shared `oCInfo` object when the info is given to the player.

## OpenGothic location

`game/game/gamescript.cpp:2086-2093` (`npc_knowsinfo`) →
`game/game/gamescript.cpp:3442-3445` (`doesNpcKnowInfo`).

`doesNpcKnowInfo` looks up the pair `(npc_symbol_index, infoInstance)` in
`dlgKnownInfos`. `npc_knowsinfo` passes `vnpc` — i.e. **the NPC argument the
script supplied** — as that key.

Meanwhile the told flag is *stored* keyed on the **dialog player (hero)**:
`exec` calls `setNpcInfoKnown(pl, info)` with `pl = player.handle()`
(`gamescript.cpp:959`, `3437-3440`), and the other readers
(`dialogChoices:882`, `npc_checkinfo:2547`) also key on the player.

## Divergence

Storage is keyed on the hero; the original lookup ignores the NPC argument.
OpenGothic's `Npc_KnowsInfo` instead keys the lookup on the passed NPC:

- `Npc_KnowsInfo(other, INFO)` with `other` == hero → key matches storage → same
  result as original. (This is the common idiom, so most calls are unaffected.)
- `Npc_KnowsInfo(self, INFO)` or `Npc_KnowsInfo(someOtherNpc, INFO)` → key is
  `(thatNpc, INFO)` while storage holds `(hero, INFO)` → OpenGothic returns
  **false even though the info was told**; the original returns **true**.

Result: any script that queries an info's told-state through a first argument
other than the hero gets a wrong (false-negative) answer, which can re-open
dialogs, re-trigger one-shot logic, or skip gated branches.

## Proposed patch

Make `npc_knowsinfo` ignore the passed NPC and query the told flag against the
dialog player (hero), matching the original's npc-independent semantics.

File: `game/game/gamescript.cpp`

OLD:
```cpp
bool GameScript::npc_knowsinfo(std::shared_ptr<zenkit::INpc> npcRef, int infoinstance) {
  auto npc = findNpc(npcRef);
  if(!npc)
    return false;

  zenkit::INpc& vnpc = npc->handle();
  return doesNpcKnowInfo(vnpc, uint32_t(infoinstance));
  }
```

NEW:
```cpp
bool GameScript::npc_knowsinfo(std::shared_ptr<zenkit::INpc> npcRef, int infoinstance) {
  auto npc = findNpc(npcRef);
  if(!npc)
    return false;

  // NOTE: in original-game Npc_KnowsInfo (FUN_006dc4a0) reads only the info
  // instance and discards the NPC argument; the told flag is a global per-info
  // value (oCInfoManager::InformationTold). The told set is keyed on the dialog
  // player, so query against the player rather than the passed NPC.
  auto hero = findNpc(vm.global_other());
  const zenkit::INpc& vnpc = (hero!=nullptr) ? hero->handle() : npc->handle();
  return doesNpcKnowInfo(vnpc, uint32_t(infoinstance));
  }
```
