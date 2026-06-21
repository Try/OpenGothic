# ext: Npc_GetInvItemBySlot — original ignores category and uses a flat 0-based slot index

**Confidence:** High (engine contract divergence verified in decompile). The *fix* is DEFERRED — see below.

## Original function + address (prose)
The `Npc_GetInvItemBySlot(npc, cat, slotnr)` external handler dispatches to `oCNpc::GetItem(int cat, int slotnr)` (at 0x732030). That method **discards the `cat` argument entirely** and forwards only `slotnr` to `oCNpcInventory::GetItem(int)` (at 0x70c450). `oCNpcInventory::GetItem` walks the inventory's intrusive linked list with a running counter starting at **0** and returns the element whose index equals `slotnr` (after a bounds check `slotnr < listCount + packedCount`; packed/stacked items are materialised on demand past the live list). The external then sets the global `ITEM` instance and `SetReturn`s the found item's amount field (oCItem+0x32c, the per-item stack count — confirmed: `oCNpcInventory::GetAmount` at 0x70c970 accumulates exactly this field, and the instance index is the *separate* field oCItem+0x330 returned by `oCItem::GetInstance` at 0x711420). When no item is found, the global `ITEM` is set to null and the return is 0.

Net original contract: category is ignored; the result is the `slotnr`-th item in raw inventory order (0-based); return value is that item's stack amount, 0 on miss.

## OpenGothic file:line
`/Users/admin/Downloads/opengothic/game/game/gamescript.cpp:2155` — `GameScript::npc_getinvitembyslot`.

## Divergence
OpenGothic translates `cat` into an `ItmFlags` category mask (the `switch(cat)` at lines 2165-2195) and calls `inventory().findByFlags(f, slotnr)` (`/Users/admin/Downloads/opengothic/game/game/inventory.cpp:1024`). `findByFlags`:
- **filters** the inventory to only items whose `main_flag` intersects the category mask, then
- returns the item where its 1-based running `found` counter equals `num` (`if(found==num) return ...`, with `found` pre-incremented so the first match is index **1**, not 0).

So for the same `(npc, cat, slotnr)` OpenGothic returns a *different* item than the original on two counts:
1. **Category filtering** — original returns the `slotnr`-th item of the *whole* inventory regardless of `cat`; OG returns the `slotnr`-th item *of that category only*.
2. **Off-by-one index base** — original counts from 0 (`slotnr==0` → first item); OG's `findByFlags` counts from 1 (`num==0` never matches → returns null; `num==1` → first match).

The return *value* (stack amount) and the `storeItem`/global-`ITEM` side effect are faithful; only the item *selection* diverges.

## Proposed patch
**DEFERRED.** Reason: the original behaviour (ignore category, flat 0-based index) is almost certainly an upstream quirk/bug, and OpenGothic's binding is a *deliberate* reinterpretation (see the in-code comment at lines 2152-2154 and 2161-2163 calling this a Gothic-1 grouped-inventory API). `Npc_GetInvItemBySlot` has effectively no callers in stock Gothic II Daedalus, so a faithful rewrite to flat 0-based indexing has no observable upside and risks regressing the intended grouped-inventory semantics that mods built against OpenGothic may rely on. A surgical, build-verifiable fix would require:
- enumerating real script callers (Gothic I + Gothic II + active mods) to decide whether flat-index parity is actually desired, and
- a flat indexed accessor on `Inventory` (the `items` vector exists — `inventory.h:157` — but no public 0-based `at()` is exposed; only `findByFlags`, `findByClass`, `getItem`).

Recommended only if a concrete script-side parity break is reproduced. Until then: documented, not patched.

If parity is later desired, the minimal change is to add a 0-based flat accessor and replace the body with a category-ignoring flat lookup, guarded by:
`// NOTE: in original-game Npc_GetInvItemBySlot @oGameExternal.cpp -> oCNpc::GetItem @0x732030 ignores 'cat' and returns the (0-based) slotnr-th inventory item; amount (oCItem+0x32c) is the return.`

## Externals checked and found faithful
- `Npc_GetStateTime` (handler FUN_006e2560 → `oCNpc::GetAIStateTime` 0x73eff0 → `oCNpc_States::GetStateTime` 0x76c0a0): original truncates a ms float and integer-divides by 1000; OG `int32_t(stateTime()/1000)` with ms ticks — faithful. `Npc_SetStateTime` (×1000) — faithful.
- `Npc_GetInvItemBySlot` **return value** (item amount, 0 on miss) and global-ITEM side effect — faithful (only selection diverges, above).
- `Npc_HasItems` (FUN_006e72e0 → `oCNpcInventory::GetAmount`): returns stack amount, 0 on null npc — matches OG `itemCount()`/0.
- `Npc_GetActiveSpell` (FUN_006e5580 → `GetActiveSpellNr` 0x73cf60): returns spell id or -1 when no active spell — matches OG -1. (Original leaves a stale return on null npc; OG returns -1 — benign.)
- `Npc_GetActiveSpellIsScroll` (FUN_006e5b20 → `GetActiveSpellIsScroll` 0x73d020 → `oCItem::MultiSlot` 0x7125a0): returns the MULTI flag of the active spell item — matches OG `isSpell()`/`isMulti()`.
- `Npc_GetReadiedWeapon` (FUN_006eda10 → `oCNpc::GetWeapon` 0x7377a0): returns the currently-drawn weapon, null if mode<=fist — consistent with OG `activeWeapon()`.
- `Hlp_GetInstanceID` / `Hlp_IsValidNpc` / `Hlp_IsValidItem`: -1 / instance index, null-check booleans — faithful.
- `Wld_GetMobState`: minor note only — original returns the raw mob state (can be negative) and prefers `GetInteractMob` over `FindMobInter(scheme)`, whereas OG uses `std::max(0, mob->stateId())` over `availableMob`. Below the high-confidence bar; not filed.
