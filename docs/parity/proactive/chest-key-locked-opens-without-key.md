# Chest: key-locked container (no pickLock string) opens without the key

**Confidence:** High

## Original function + address

`oCMobLockable::CanOpen` (Gothic2.exe `0x007244f0`) is the gate the original
runs before a locked mob may be opened (`oCMobContainer` derives from
`oCMobLockable`, whose `Open` at `0x00726500` is only reached after the
interact/`CanOpen` path succeeds). In prose, `CanOpen` does:

- If the lockable is not locked (`locked` flag clear), return success.
- Compute `hasKey` = "this lockable requires a key instance" AND
  `oCNpc::IsInInv(keyInstance)` is non-null (player owns the key).
- Compute `hasLockpick` = "this lockable has a non-empty pickLock string" AND
  `oCNpc::IsInInv("ITKE_LOCKPICK")` is non-null (player owns a lockpick).
- Return success only if `hasKey || (hasLockpick && HasTalent(PICKLOCK >= 1))`
  (`oCNpc::HasTalent` at `0x00731990`, talent id 5 = picklock).
- Otherwise it fails: it posts a manipulate/`T_DONTKNOW` message and the
  container does NOT open.

Consequence in the original: a container that is **locked, requires a key, and
has no pickLock string** can be opened **only** by a player who carries the key.
There is no fallback — no key means it stays shut.

## OpenGothic file:line

- Open path / missing gate: `game/game/playercontrol.cpp:326-329`
  (`PlayerControl::interact(Interactive&)` calls `inv.open(*pl,it)` directly for
  containers, never consulting the key/lock condition).
- Effective gate: `game/ui/inventorymenu.cpp:166-177` (`InventoryMenu::open`),
  which only branches on `Interactive::needToLockpick`.
- Root predicate: `game/world/objects/interactive.cpp:571-576`
  (`Interactive::needToLockpick`).
- The correct key/lock logic already exists but is unused on this path:
  `Interactive::checkUseConditions`, `game/world/objects/interactive.cpp:687-743`.

## Divergence

`needToLockpick` only decides whether to start the lock-picking minigame:

```cpp
bool Interactive::needToLockpick(const Npc& pl) const {
  const size_t keyInst = keyInstance.empty() ? size_t(-1) : world.script().findSymbolIndex(keyInstance);
  if(keyInst!=size_t(-1) && pl.inventory().itemCount(keyInst)>0)
    return false;
  return !(pickLockStr.empty() || isLockCracked);
  }
```

For a container that is **key-locked with an empty `pickLockStr`** and a player
who does **not** hold the key:

- The first branch is skipped (`itemCount(keyInst)==0`).
- The return evaluates `!(pickLockStr.empty() || isLockCracked)` =
  `!(true || ...)` = **`false`**.

So `needToLockpick` returns `false`, `InventoryMenu::open` takes the `else`
branch and sets `state = State::Chest`, and the chest opens — even though the
player has no key. `PlayerControl::interact` reaches `inv.open` without ever
calling `checkUseConditions` (which would have returned false and printed the
"missing key" message, faithfully matching `CanOpen`). Net effect: **key-only
locked chests are free to loot in OpenGothic**, contrary to `CanOpen`.

(Note the inverse case — a chest with a pickLock string — is handled, because
`needToLockpick` returns true and routes to the lock-picking minigame; and a
chest carrying both a key and a pickLock string is also covered. Only the
key-required / no-pickLock-string / no-key combination escapes the gate.)

## Proposed patch

Gate the container-open path through the already-correct `checkUseConditions`,
mirroring the original `CanOpen` decision (key OR usable-lockpick, else refuse +
HUD message). `checkUseConditions` is `private` (interactive.h:134); expose a
thin public wrapper so the external open path can consult it without duplicating
logic. All referenced symbols are grep-verified to exist.

`game/world/objects/interactive.h` (make the gate reachable from the open path):

```cpp
// OLD
    bool                needToLockpick(const Npc& pl) const;
// NEW
    bool                needToLockpick(const Npc& pl) const;
    // NOTE: in original-game oCMobLockable::CanOpen (Gothic2.exe 0x007244f0) a locked
    // container opens only when the player owns the key, or owns a lockpick AND has the
    // picklock talent; otherwise it refuses. Expose the existing key/lock condition so
    // the container-open path can run it (key-only chests must not open without the key).
    bool                canOpen(Npc& npc) { return checkUseConditions(npc); }
```

`game/game/playercontrol.cpp` (refuse to open when the key/lock gate fails):

```cpp
// OLD
  if(it.isContainer()){
    inv.open(*pl,it);
    return true;
    }
// NEW
  if(it.isContainer()){
    if(!it.canOpen(*pl))
      return true; // locked: checkUseConditions printed the missing-key/lockpick hint
    inv.open(*pl,it);
    return true;
    }
```

`checkUseConditions` (interactive.cpp:687-743) already emits the correct HUD
messages via `GameScript::printMobMissingKey` /
`printMobMissingKeyOrLockpick` / `printMobMissingLockpick` (all verified in
`game/game/gamescript.h:129-131`), and it sets `isLockCracked` when the player
holds the key — matching the original's "open only with key" semantics for the
key-only chest. A container with a pickLock string still satisfies
`checkUseConditions` (via `needToPicklock && canLockPick`) and proceeds to
`inv.open`, where `needToLockpick` then routes to the lock-picking minigame as
before, so the existing pickable-chest flow is preserved.

### Verification note / residual risk

`checkUseConditions` also evaluates `conditionFunc` and `useWithItem`. For
containers those are normally unset, so behavior is unchanged in the common
case; if a future container ships a `conditionFunc`, this gate would (correctly,
per the original `CanInteractWith`/`CanOpen` chain) also honor it. If a reviewer
prefers to limit the new gate strictly to the key/lock check, the alternative is
to extend `needToLockpick`'s key-only branch — but `checkUseConditions` is the
faithful analogue of the original gate and avoids duplicating the
key/lockpick/talent logic, so it is preferred.
