# Issue #841 — Implement missing `PLAYER_` callbacks

**Category:** scripting / mob-use callbacks
**Disposition:** OUT-OF-SCOPE for parity (with a small optional FIX path; see below)

## Requested callbacks
- `PLAYER_MOB_NEVER_OPEN`, `PLAYER_MOB_WRONG_SIDE`  (claimed "currently missing")
- `PLAYER_RANGED_NO_AMMO`  (missing, but vanilla scripts have no body)
- `PLAYER_STEAL_*` x4  (from unreleased sequel — not in G2 at all)

## Key parity finding (Ghidra, Gothic2.exe)
The string table contains all PLAYER_MOB_* names:
- `PLAYER_MOB_NEVER_OPEN`  @ `0x0088dcbc`
- `PLAYER_MOB_WRONG_SIDE`  @ `0x0088dcec`

But **neither string has any code xref** in Gothic2.exe. The original G2 engine
*declares* these two callbacks yet **never invokes them** (unlike
`PLAYER_MOB_MISSING_KEY` `0x0088dc00`, `..._TOO_FAR_AWAY` `0x0088dcd4`,
`..._ANOTHER_IS_USING` `0x0088dd1c`, which are all referenced and wired). So for
parity-with-original-G2, adding these two callbacks is **not required** — the
reference engine does not fire them either.

The `PLAYER_STEAL_*` callbacks are sequel-only and have no presence in G2.
`PLAYER_RANGED_NO_AMMO` is untestable (vanilla scripts ship no implementation).

## OG current state (already wired callbacks)
- `game/game/gamescript.cpp:996–1059` — `printMobMissingItem`,
  `printMobMissingKey`, `printMobAnotherIsUsing`, `printMobMissingKeyOrLockpick`,
  `printMobMissingLockpick`, `printMobTooFar`. Each looks up the lower-cased
  symbol, falls back to `T_DONTKNOW` (G1) or nothing.
- Call sites: `game/world/objects/interactive.cpp:687–711` (missing key/lockpick/
  item), `:792` (too far), `:838/:847` (another is using).

## Why OUT-OF-SCOPE
The only G2-testable members of the request (`NEVER_OPEN`, `WRONG_SIDE`) are
dead in the reference engine, so implementing them would *exceed* original
behavior rather than match it. The rest are sequel/untestable. There is no
parity divergence to close.

## Optional, low-risk additive path (NOT parity-required)
If maintainers still want them as a QoL behavior (clearly beyond original),
the plumbing is a trivial mirror of the existing helpers. In
`game/game/gamescript.cpp`, add e.g.:
```
// NOTE: in original-game PLAYER_MOB_WRONG_SIDE exists as a symbol but is never
// called by the engine (no xref in Gothic2.exe); this is an additive QoL hook.
void GameScript::printMobWrongSide(Npc& npc) {
  auto id = vm.find_symbol_by_name("player_mob_wrong_side");
  if(id==nullptr) { if(owner.version().game==1)
      owner.player()->playAnimByName("T_DONTKNOW", BS_NONE); return; }
  ScopeVar self(*vm.global_self(), npc.handlePtr());
  vm.call_function<void>(id);
  }
```
Call site for WRONG_SIDE would be in `Interactive::checkUseConditions` /
side-attach logic (`reverseState`/`attachMode`, interactive.cpp:335,752);
NEVER_OPEN where a lock exists but no key/lockpick is configured. Both require
deciding semantics not defined by the original — hence not a parity FIX.
