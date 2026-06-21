# Issue #177 — G2 NotR weapon smithing: multiple errors

## Issue (three reported sub-bugs)
1. Selling a self-made weapon to Harad also sells the currently equipped weapon
   (e.g. El-Bastardo).
2. Forging a sword while holding several blade pieces (e.g. 5) creates one sword
   but the other four pieces vanish.
3. "More smithing issues" (unspecified) + same symptom while cooking.

Mod context: reporter & confirmer run **L'Hiver Edition 0.9**. Per the comment
thread, Try fixed (1) [typo in `npc_getequippedmeleeweapon`] and (2) [double
consume at anvil] back in 2022; the residual is a crafting/cooking
animation-event sequencing problem ("both hot blades consumed", rapid-click
spawning items) that Try's last comment pinned to stove/forge anim-event order.
Underlying mechanic is vanilla-reproducible (NotR smithing exists in vanilla
G2), but the specific crash reports came through a mod save.

## Subsystem & OG files
- `game/game/gamescript.cpp:2315` `npc_getequippedmeleeweapon` (was the typo for
  case 1) and `:194-196` bindings.
- `game/game/inventory.cpp:312-339` `Inventory::transfer` (sell/trade path),
  `:281-310` `delItem`, `:223-272` `addItem` (stack merge), `:817-906` equip.
- `game/world/objects/item.cpp:162-171` `setAsEquipped` (equipped vs amount).
- Crafting/cooking is script-driven via mobsi interaction (`AI_UseMob`,
  `Use_Mob`) + anim events (`game/world/objects/npc.cpp` processEvents loop,
  ~2214) and `Npc::setAnimItem` / item `on_state` callbacks.

## Original behavior (Gothic2.exe — prose)
Crafting consumes input items at a specific **animation event** during the forge
loop (one consume per completed anvil/stove cycle), gated by the mob interaction
state machine (`oCMobInter`/`oCMobSmith` use sequences). The equipped weapon is a
distinct logical instance: `oCNpcInventory` tracks an equipped flag per slot
(see `oCItem::MultiSlot`/`SplitSlot` @0x007125a0/0x00712610) so that selling a
stack of N identical weapons does not implicitly drag the equipped one unless it
is the item being transferred. Selling is `oCNpcInventory::Remove`/transfer that
operates on the *unequipped* portion of a stack first.

## OpenGothic current behavior (file:line)
- A stack of identical self-made weapons is ONE `Item` with `amount=N` and an
  `equipped` counter (`item.cpp:314-320`, `162-171`). `transfer`
  (`inventory.cpp:324-337`) treats the whole stack uniformly: selling the full
  count unequips it (line 325-331); selling a partial count (line 334-336) just
  decrements `amount` via `to.addItem(itemSymbol,count)` without consulting the
  equipped sub-count. There is no logic to prefer the unequipped instances, so a
  partial sale can leave `equipped > amount` (the `setAsEquipped` consistency
  warning at item.cpp:166-168 exists precisely for this).
- Crafting consume happens in script through anim-event callbacks; if the forge
  sequence fires its consume event more than once per intended cycle, surplus
  pieces are removed (case 2 residual) — matching Try's stove/forge anim-event
  observation.

## Divergence
The stack-with-equipped-subcount model collapses the "equipped instance" and the
"sellable instances" into one object, so trade/forge operations that should
target unequipped instances can touch the equipped one or mis-count consumed
pieces. The remaining defect is the crafting consume firing per anim-event rather
than per completed cycle.

## Disposition: DEFER
Two of three reported items were already patched upstream; the residual is a
crafting-animation-event timing bug plus a stack/equipped-subcount accounting
edge case. A correct fix needs: (a) `transfer`/sell to subtract from the
*unequipped* portion first (only unequip when the requested count exceeds
`amount-equipped`), and (b) the forge/stove consume to fire once per completed
interaction cycle, not per raw anim event. Both require in-game testing against a
NotR/L'Hiver save (the repro is mod-gated and not runnable headless here), so no
surgical patch is proposed. Investigation guide above stands.
