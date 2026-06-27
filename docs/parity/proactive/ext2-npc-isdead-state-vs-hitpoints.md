# External Npc_IsDead tests the ZS_Dead AI-state; original tests HITPOINTS < 1

**Confidence:** Medium-High. The code-level divergence is unambiguous (state-machine
predicate vs raw hitpoints). The observable window in vanilla is narrow because
OpenGothic synchronises the ZS_Dead state with every HP-reduction path, but two
script-reachable cases diverge cleanly (see below). The proposed fix is surgical
(external handler only) and strictly moves toward the original with no regression
in the normal death path.

## Original function + address

- `Npc_IsDead` external handler (Gothic2.exe `0x006e8960`, registered in
  `DefineExternals_Ulfi`): when the resolved NPC pointer is null it returns `1`
  (true); otherwise it forwards `oCNpc::IsDead`.
- `oCNpc::IsDead` (Gothic2.exe `0x00736740`) is a one-line accessor: it returns
  `HITPOINTS < 1`, i.e. true exactly when the NPC's current hitpoints attribute is
  `<= 0`. It inspects no AI state, no bodystate, no unconscious flag — purely the
  hitpoints value.

So in the original, "dead" is a pure hitpoints predicate, and the engine-internal
`IsDead` and the script-visible `Npc_IsDead` are one and the same HP test.

## OpenGothic file:line

- `game/game/gamescript.cpp:2179` `GameScript::npc_isdead` returns
  `npc==nullptr || isDead(*npc)`.
- `game/game/gamescript.cpp:1340` `GameScript::isDead(const Npc&)` returns
  `pl.isInState(ZS_Dead)`.
- `game/world/objects/npc.cpp:4631` `Npc::isInState` returns
  `aiState.funcIni==stateFn`.

That is, OpenGothic reports "dead" only when the NPC is *currently executing the
ZS_Dead AI script-state*, not when its hitpoints are `<= 0`.

## Divergence

The original answers a hitpoints question; OpenGothic answers a state-machine
question. They disagree whenever hitpoints and the ZS_Dead state are out of sync:

1. **Hitpoints > 0 but still in ZS_Dead (revival).** `Npc::checkHealth`
   (`npc.cpp:560`) early-returns `if(isDead()) return false;`, so feeding a dead
   NPC positive hitpoints via `Npc_ChangeAttribute(npc, ATR_HITPOINTS, +n)` raises
   its HP but never leaves ZS_Dead. Original `Npc_IsDead` then returns **false**
   (HP>0); OpenGothic returns **true** (still ZS_Dead).

2. **Hitpoints <= 0 but never routed through `onNoHealth`.** Any NPC whose
   hitpoints are at/below zero without the death pipeline having run (e.g. an NPC
   placed/initialised with 0 HP, or queried before its first AI processing) reads
   as dead in the original (HP<1) but as alive in OpenGothic (funcIni != ZS_Dead).

In the common death paths (lethal `takeDamage`, `Npc_ChangeAttribute` damage,
regen tick) OpenGothic calls `onNoHealth` -> `startState(ZS_Dead)`, which sets
`aiState.funcIni = ZS_Dead` synchronously (`npc.cpp:3129`), so those agree — which
is why the bug is easy to miss. The two cases above are the residual, script-
reachable divergence; in all of them the original is hitpoints-driven.

## Proposed patch

Make the external mirror the original's `oCNpc::IsDead` hitpoints test directly.
This is external-only and does not touch the engine-internal `GameScript::isDead`
/ `Npc::isDead` that OpenGothic relies on for its own ZS_Dead state bookkeeping.

Grep-verified symbols: `Npc::attribute(Attribute)` (`npc.h:216`), `ATR_HITPOINTS`
(`constants.h:473`), the existing `attribute(ATR_HITPOINTS)` call pattern
(`npc.cpp:2281`).

OLD (`game/game/gamescript.cpp:2179`):

```cpp
bool GameScript::npc_isdead(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  return npc==nullptr || isDead(*npc);
  }
```

NEW:

```cpp
bool GameScript::npc_isdead(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  // NOTE: in original-game oCNpc::IsDead (Gothic2.exe 0x00736740, forwarded by the
  // Npc_IsDead handler 0x006e8960) returns HITPOINTS<1 (HP<=0) -- a pure hitpoints
  // test, NOT the ZS_Dead AI-state. OpenGothic's isDead() checks isInState(ZS_Dead),
  // which disagrees when HP and the death state are out of sync (e.g. an HP-revived
  // NPC still in ZS_Dead, or an NPC at HP<=0 that never ran onNoHealth). A null
  // pointer is reported dead, matching the original handler's null-path return of 1.
  return npc==nullptr || npc->attribute(ATR_HITPOINTS)<=0;
  }
```

This strictly converges on the original: a normally-killed NPC (HP=0 and ZS_Dead)
still reads dead, an unconscious NPC (HP=1, ZS_Unconscious) still reads alive, and
the two out-of-sync cases now match retail.
