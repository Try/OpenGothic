# Item-effect attribute changes don't re-validate equipment usability for non-player NPCs

**Confidence:** Low-Medium (divergence is decompiler-proven and grep-verified; player-visibility is
small in vanilla G2, and OpenGothic already carries a deliberate "gothic doesn't care" simplification
here — hence **DEFERRED**, not applied.)

## Original function + address (prose)

`oCNpc::AddItemEffects` (Gothic2.exe `0x007320f0`) and its mirror `oCNpc::RemoveItemEffects`
(`0x00732270`) are the two routines that apply / revoke an item's on-effect bonuses
(`protection[]`, the positive-`change_atr[i]`/`change_value[i]` attribute deltas via
`ChangeAttribute` `0x0072ff60`, `disguise_guild`, and the item's `on_equip`/`on_unequip` script
function). Both end their body with an **unconditional** call to `oCNpc::CheckAllCanUse`
(`0x00731de0`). The xref sets confirm the reach: `AddItemEffects` is called from `Equip`
(`0x0073a003`), `EquipItem` (`0x0073246e`) **and** the consume path `UseItem` (`0x0073bd0a`);
`RemoveItemEffects` from `Equip` (`0x00739e82`), `UnequipItem` (`0x00732702`) and `DoDropVob`.

`CheckAllCanUse` walks every equipped slot (`this+0x9b0`) and the inventory, and for each item that
is equippable (`oCItem::HasFlag(0x40000000)`) but now fails `CanUse` (`0x007319b0`) it calls
`UnequipItem` **and** `DisplayCannotUse`, looping to a fixpoint (re-entrancy-guarded by the static
`DAT_00ab26a8`). It runs for **every** NPC — there is no `this == player` gate. So when an item's
effect lowers an attribute (e.g. unequipping / consuming an item that had carried a positive
`change_atr`, or equipping one whose `change_value` is negative), any *other* equipped item whose
`cond_atr` requirement is no longer met is auto-unequipped with the "cannot use" feedback, for
player and non-player NPCs alike.

## OpenGothic file:line

`game/game/inventory.cpp:1041-1054` — `Inventory::invalidateCond(Npc&)`, OpenGothic's stand-in for
`CheckAllCanUse`. It is invoked from `game/world/objects/npc.cpp:1294-1295`
(`Npc::changeAttribute`, only when `val<0`), not from the effect-application helper itself.

## Divergence

`Inventory::invalidateCond` early-returns for non-players:

```cpp
void Inventory::invalidateCond(Npc &owner) {
  if(!owner.isPlayer())
    return; // gothic doesn't care
  ...
```

So for a non-player NPC, an item-effect attribute drop never re-validates its equipment: in the
original such an NPC would have a now-unqualified weapon/armor stripped by `CheckAllCanUse`;
OpenGothic leaves it equipped. (For the **player** the behavior is effectively reached — the
`val<0` gate in `changeAttribute` fires `invalidateCond`, and the cascade emerges naturally because
each forced `unequip` runs `applyArmor(-1)` → `changeAttribute(val<0)` → re-entrant `invalidateCond`,
matching the original's fixpoint loop. The remaining player-side gap is only the missing
`DisplayCannotUse` message on an auto-unequip.)

## Proposed patch — DEFERRED

Reason: the only vanilla G2 trigger for an item lowering an attribute is the very rare
negative-`change_value` item or the (deferred) `AddItemEffects`-on-consume path; combined with the
non-player-only scope, in-game visibility is minimal. OpenGothic's `if(!owner.isPlayer()) return;`
is an intentional, documented simplification ("gothic doesn't care"), and dropping it would re-run
cond re-validation for every monster/human on every negative attribute tick — a behavioral and
performance change wider than this note can verify as net-positive. The player-visible sliver
(missing cannot-use feedback message on auto-unequip) belongs to the UI-message subsystem, not the
effect-application logic, and is better addressed there.

If pursued, the faithful change would be to (a) drop the player-only early-return so non-player NPCs
also re-validate, and (b) emit `printCannotUseError`/the cannot-use message inside
`invalidateCond(Item*&,Npc&)` when a slot is force-unequipped, with:

```cpp
// NOTE: in original-game oCNpc::CheckAllCanUse @0x00731de0 (called unconditionally from
// AddItemEffects @0x007320f0 and RemoveItemEffects @0x00732270) re-validates equipped items for
// EVERY NPC after an item-effect attribute change, auto-unequipping (with DisplayCannotUse) any
// item whose cond_atr is no longer met. OpenGothic skips this for non-player NPCs.
```
