# Consume effect applied at use-start instead of the drink/eat swallow frame

**Confidence:** High on the divergence (original behavior is well-evidenced); **DEFERRED** on the fix (the corrective change is structural, not surgical, and overlaps the already-deferred multistate consume machine).

## Original fn + address

The food/drink/potion consume is driven, per AI tick, by `oCNpc::EV_Drink` (Gothic2.exe `0x00753463`), the handler for the `oCMsgUseItem` message that both the player's inventory "use" and `AI_UseItem` enqueue. `EV_Drink` polls the active eat/drink animation's progress via `zCModel::GetProgressPercent` and walks a 3-phase state machine keyed on three progress thresholds (IEEE-754 doubles in the function): **0.15**, **0.65**, **0.85**.

- Phase 0 -> at progress >= **0.15** the item is materialized into the hand slot (`PutInSlot`, NPC_NODE_LEFTHAND).
- Phase 1 -> at progress >= **0.65** it calls `oCNpc::UseItem` (`0x0073bc10`). UseItem is where the FOOD-flag (0x20) nutrition heal `ChangeAttribute(ATR_HITPOINTS, nutrition)` (`0x0072ff60`) and `AddItemEffects` run; the on_state effect script runs through the sibling `oCNpc::EV_UseItemToState` (`0x007558f0`) via `oCItem::GetStateEffectFunc` (`0x00712b80`).
- Phase 2 -> at progress >= **0.85** the item count is decremented and the stack consumed/removed from inventory.

So in the original the actual consume *effect* (HP/nutrition heal, potion effect, on_state) lands at roughly two-thirds through the swallow animation, and the item is only removed near the very end (~0.85).

## OG file:line

`/Users/admin/Downloads/opengothic/game/game/inventory.cpp:1013-1034` (`Inventory::use`):
- line 1013 `setAnimItem(...)` merely *starts* the cosmetic animation (and arms `implAniWait` on the AI queue);
- line 1021-1022 applies the FOOD nutrition heal `changeAttribute(ATR_HITPOINTS, nutrition)` immediately;
- line 1026-1034 invokes `on_state[0]` (the potion/food effect script) immediately.

The item itself *is* removed later, correctly, at the `ITEM_DESTROY` animation event (`/Users/admin/Downloads/opengothic/game/world/objects/npc.cpp:2434` -> `Inventory::clearSlot(..., remove=true)`), which mirrors the original ~0.85 removal frame.

## Divergence

OpenGothic applies the consume *effect* (nutrition heal + `on_state[0]`) synchronously at animation **start** (progress ~0.0) inside `Inventory::use`, whereas the original applies it at **~0.65** progress of the eat/drink animation (the swallow frame) inside `EV_Drink`/`UseItem`. The item-removal timing already matches (anim event ~0.85), but the effect timing does not. Observable consequences: the heal/buff registers a fraction of a second too early, and an eat/drink that is aborted after start (e.g. the player moving) cannot occur effect-free in OG the way it can in the original, because in OG the effect has already fired before the swallow frame is reached.

## Proposed patch

**NO FINDING (surgical).** A faithful fix requires deferring the `changeAttribute(ATR_HITPOINTS, nutrition)` and `invokeItem(on_state[0])` calls out of `Inventory::use` and re-firing them when the eat/drink animation crosses ~0.65 progress. OpenGothic has no existing 0.65-progress hook (its anim-event-driven consume path only exposes the MDS `ITEM_DESTROY`/`ITEM_INSERT` events, which map to the ~0.15/~0.85 frames, not the 0.65 effect frame), so replicating `EV_Drink`'s `GetProgressPercent`-threshold gating would mean adding a progress-based deferred-effect mechanism with item/state bookkeeping. That is a structural change, it risks regressions in the food-heal and on_state paths that were only recently corrected, and it overlaps the already-deferred onstate-multistate consume machine. Hold for a dedicated consume-timing pass rather than a one-line edit.

```
// NOTE: in original-game oCNpc::EV_Drink (Gothic2.exe 0x00753463) the consume effect is gated on
// eat/drink animation progress: hand-insert at >=0.15, oCNpc::UseItem @0x0073bc10 (nutrition heal
// @0x0072ff60 + AddItemEffects) and the on_state script (EV_UseItemToState @0x007558f0 via
// GetStateEffectFunc @0x00712b80) at >=0.65, item removal at >=0.85. OpenGothic's Inventory::use
// applies the heal + on_state[0] at animation start (progress ~0) instead of the 0.65 swallow frame.
```
