# Multi-attacker combat coordination — engine-side parity sweep

**Confidence:** NO FINDING (no surgical engine-side divergence)

## Scope
How multiple NPCs attacking the same target coordinate: attacker slots / "wait your
turn", gang-up positioning, surround/flank, shared-target tracking, don't-all-swing
timing, formation spacing.

## What was checked in Gothic2.exe

- `oCNpc::ThinkNextFightAction` @0x0067e350 (the engine combat tick). It operates
  **exclusively on the single focus enemy** stored at `this+0x498`: every range test
  (`IsInFightRange`), angle test (`GetAngles`), movement message (`oCMsgMovement`) and
  attack message (`oCMsgAttack`) targets `*(this+0x498)`. It never reads the enemy-list
  count (`this+0x9a0`) nor the enemy-list array (`this+0x998`) to gate or stagger
  attacks. There is no engine "only N attackers engage" slot/ring logic and no
  cross-attacker timing here.
- `oCNpc::FindNextFightAction` @0x0067d680 → faithfully mirrored by
  `game/game/fightalgo.cpp` (`FightAlgo::fillQueue`). This is single-NPC vs single
  target; it picks a fight-AI move table by range/focus/body-state only.
- `oCNpc::GetNextEnemy` @0x00734e30 → target selection (nearest valid hostile in the
  per-NPC enemy list, with retention of the current focus `this+0x498`). Maps to
  `Npc::updateNearestEnemy` (`game/world/objects/npc.cpp:2357`). This is per-NPC
  perception/target-pick, not coordination among co-attackers.
- `oCNpc::IsEnemyBehindFriend` @0x00741600 — the one genuinely coordination-flavored
  engine routine (find a friendly NPC standing in the attack area so the attacker
  doesn't charge/swing through an ally). **It has no xrefs in Gothic2.exe**
  (`wde xrefs 00741600` → "no xrefs"), is registered under no Daedalus-external string
  (`wde strings BehindFriend` → none), and is therefore dead code in G2. Reimplementing
  it in OpenGothic would change no observable behavior — gold-plating, not parity.
- `oCNpc::IsLastTarget` is a red herring: @0x007b2d80 is `zCRoute::IsLastTarget`
  (waynet), unrelated to combat; it also has no xrefs.

## Conclusion
The engine combat path is strictly single-attacker-vs-single-focus. All multi-attacker
coordination (attacker slots / wait-your-turn, surround, gang-up spacing, shared-target
selection) is driven by Daedalus — fight-AI move tables (`my_w_nofocus` etc.) plus the
`B_`/`ZS_` combat states — which OpenGothic runs via the same scripts. The only engine
routine touching ally-aware positioning (`IsEnemyBehindFriend`) is unused in Gothic2.exe.
OpenGothic's `FightAlgo` and `Npc::implAttack`/`updateNearestEnemy` already reflect the
engine's single-target model faithfully (and prior divergences in those paths are already
annotated/fixed).

No surgical, build-verifiable engine-side divergence in multi-attacker coordination.
NO FINDING.
