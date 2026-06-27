# Trade: items sold to a merchant permanently join its inventory (should vanish on trade close)

**Confidence:** Medium-High (decompilation of the trade-container lifecycle is unambiguous and
matches documented Gothic behavior; residual uncertainty is only about the exact backing of the
merchant's offered-item view). The *finding* is solid; the *fix* is structural, so it is DEFERRED.

## Original function + address (prose only)

- `oCViewDialogTrade::SetNpcLeft` @ `0x0068b180` and `oCViewDialogTrade::SetNpcRight` @ `0x0068b290`,
  run when a trade window opens. Each one creates a brand-new `oCItemContainer` (allocated fresh) and
  stores it on the corresponding NPC at object-offset `0x734`. `SetNpcLeft` additionally allocates a
  fresh global `oCNpc::stealcontainer` (`oCStealContainer`), gives it the merchant as owner, and binds
  it to the left view panel (`oCViewDialogStealContainer`); the right panel
  (`oCViewDialogInventory`) is bound to the player's real inventory.
- `oCViewDialogTrade::OnTransferLeft` @ `0x0068b840` is the SELL path (player -> merchant). It removes
  the selected item from the player's inventory view, computes the sell price
  (`ROUND(multiplier * GetValue)`), hands the player a freshly created currency item, and inserts the
  *sold item into the merchant's `oCStealContainer`* — never into the merchant's real `oCNpcInventory`.
- `oCViewDialogTrade::OnTransferRight` @ `0x0068bb10` is the BUY path; it pulls the item back out of the
  same `oCStealContainer`, so buy-back works *within the session*.
- `oCViewDialogTrade::OnExit` @ `0x0068c000` releases both NPCs' `0x734` containers (and the global
  steal container is released/replaced on the next `SetNpcLeft`), setting the pointers to 0.

Net effect in the original: the merchant's tradeable list during a session is a temporary
`oCStealContainer`, and anything the player *sells* is dropped into that temporary container. When the
trade window closes the container is destroyed, so sold items disappear and are not part of the
merchant's persistent `oCNpcInventory`. This is the well-known Gothic rule that merchants cannot be
used as storage: sell an item, close trade, reopen — the item is gone. Items the player *buys* are
genuinely removed and stay removed (the steal container reflects the merchant's real stock).

## OpenGothic file:line

- `game/world/objects/npc.cpp:3597` `Npc::sellItem` -> `Inventory::transfer(to.invent, invent, ...)`
  moves the sold item straight into the merchant's **real** `invent`.
- `game/game/inventory.cpp:319` `Inventory::transfer` -> `to.addItem(...)` (lines 343/347) inserts into
  the destination's persistent item vector.
- `game/ui/inventorymenu.cpp:132` `InventoryMenu::trade` builds the merchant page from
  `tr.inventory()` (the real, serialized inventory; `TradePage` at inventorymenu.cpp:56).
- `game/ui/inventorymenu.cpp:98` `InventoryMenu::close` does nothing to revert the merchant inventory;
  there is no per-session snapshot/restore anywhere (grep for backup/snapshot/restore is empty).

## Divergence

In OpenGothic the merchant's trade list *is* its real persistent `Inventory`, so an item the player
sells is permanently added to the merchant and survives closing/reopening the trade window and a
save/load cycle. In the original the sold item lives only in a session-temporary `oCStealContainer`
that is freed on `OnExit`, so it vanishes when trade closes. Buy-back inside one session behaves the
same in both; the difference is purely the cross-session/cross-save persistence of sold goods (and,
symmetrically, OpenGothic letting the player stash items with a merchant for safekeeping).

## Proposed patch

DEFERRED.

Reason: a faithful fix requires modeling the merchant's offered-item list as a session-temporary
container that is *separate* from the merchant's serialized `Inventory`:
- sold items must go into that temporary container (so they are still buy-back-able in the same
  session) but be discarded by `InventoryMenu::close` / when the trade ends;
- the merchant trade page must present the union of the real inventory (for buying) plus the temporary
  sold-items container;
- items the player *buys* must still come out of the real inventory.

OpenGothic currently has no such temporary-container concept in `Inventory`/`InventoryMenu`, and
`Inventory::transfer` is shared with chest/NPC-to-NPC moves, so a correct change touches the trade
data model rather than a single expression. A naive surgical edit (e.g. dropping the `to.addItem` on
sell) would break same-session buy-back and mis-bill the player, so it does not meet the
high-confidence, build-verifiable, surgical bar. Recommend implementing the temporary trade-container
model as a dedicated change.

// NOTE: in original-game oCViewDialogTrade::OnTransferLeft @0x0068b840 inserts the sold item into the
// merchant's temporary oCStealContainer (created in SetNpcLeft @0x0068b180, freed in OnExit
// @0x0068c000), never into the merchant's persistent oCNpcInventory.
