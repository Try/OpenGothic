# Issue #656 — Npc runtime corner cases

## Issue
Maintainer-authored tracking issue listing three unresolved design questions about NPC guild/monster classification:
1. **Auto-equip:** separation by `isMonster` is "fundamentally incorrect"; changing `C_Npc.guild` has no effect, so the real determinant should be "true guild."
2. **Dragon corpses:** vanilla keeps dragon bodies (story-critical), but console-spawned dragons are auto-removed; explicit guild checks may be too rigid given modder freedom.
3. **Inventory access:** only humans open the inventory menu; but skeletons/goblins also shouldn't, while orcs/draconians should — the assumption is that the eligibility boundary is "true guild outside the `isMonster()` range."

## OG files
- game/world/objects/npc.cpp: `isMonster()` npc.cpp:1286, `isHuman()` npc.cpp:1293, `guild()` npc.cpp:1283, `trueGuild()`/`setTrueGuild()` npc.cpp:1299-1307, corpse/undead logic around npc.cpp:3153-3161, dragon special-case npc.cpp:478.
- game/game/gamescript.cpp (auto-equip / best-weapon selection), game/world/objects/inventory.cpp (equip), inventory menu gating in the UI layer.

## Original behavior (prose)
In the original, monster-vs-human behavior (auto-equip, inventory UI, corpse persistence) is driven by the NPC's effective/"true" guild rather than the live `guild` symbol the script may mutate. The `isMonster` range test on the current `guild()` is a coarse proxy that diverges whenever scripts reassign guild at runtime, which is why mutating `C_Npc.guild` produces no behavioral change in OG (it reads the true guild instead).

## OG current file:line
- `isMonster()` keys off `guild()` (the clamped live symbol), npc.cpp:1286-1290, rather than `trueGuild()` (npc.cpp:1303). `trueGuild()` exists but is only used in select corpse/undead paths, not in `isMonster`/`isHuman`/auto-equip/inventory gating.

## Divergence
The three behaviors are gated on `isMonster()`/`guild()` instead of a consistent `trueGuild()`-based classification, so runtime guild changes don't propagate, dragon-corpse persistence relies on a narrow guild check, and inventory access doesn't match the orc/draconian-yes, skeleton/goblin-no split.

## Recommendation: DEFER
This is an open design/refactor issue, not a surgical parity fix. Guidance:
1. Introduce a `trueGuild()`-based monster/human classifier and route auto-equip, inventory-menu gating, and corpse-persistence through it (audit every `isMonster()`/`guild()` call site for which want true vs live guild).
2. Validate the orc/draconian-inventory vs skeleton/goblin-no-inventory split against the GIL_* ordering for both G1 and G2 separators.
3. For dragon corpses, prefer the corpse-persistence rule the original uses (true-guild based) over a hardcoded `GIL_DRAGON` check so console/mod spawns behave like story dragons.
4. Needs runtime testing with the maintainer's test cases; too broad and behavior-sensitive for a confident one-shot patch.
