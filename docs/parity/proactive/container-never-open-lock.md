# Container parity: locked chest with no key and no pick-combination is openable in OpenGothic

**Confidence:** Medium-High (logic divergence is certain from the decompile; reachability depends on shipped VOB data that authors a `locked` container without a key or pick-string).

## Original function + address

`oCMobLockable::CanOpen` (Gothic2.exe `0x007244f0`), the gate reached through
`oCMobLockable::CanInteractWith` (`0x00723cc0`) when the player tries to use a chest/door.

Behaviour of the original, in prose:

- If the lockable is **not locked** (state dword `+0x234`, bit 0 clear) it returns `1` (openable) immediately, regardless of key/pick.
- If it **is locked**, it computes:
  - `hasKey`  = `keyInstance` is non-empty **and** that item is in the NPC's inventory,
  - `hasPick` = `pickLockStr` is non-empty **and** the NPC carries an `ItKe_lockpick`,
  - and the PICKLOCK talent.
  It returns `1` (openable) iff `hasKey || (hasPick && talent)`.
- Otherwise it picks one failure message and returns `0` (cannot open). The message selection (script symbols verified by reading the binary's `.rdata`):
  - key **and** pick set → `PLAYER_MOB_MISSING_KEY_OR_LOCKPICK` (or `PLAYER_MOB_MISSING_KEY` when the player holds a lockpick but lacks the talent),
  - key only → `PLAYER_MOB_MISSING_KEY`,
  - pick only → `PLAYER_MOB_MISSING_LOCKPICK`,
  - **neither key nor pick** → `PLAYER_MOB_NEVER_OPEN`, and **returns 0 (refuses to open)**.

The "neither" branch is the divergence: a container that is `locked=TRUE` but has an
empty `keyInstance` **and** an empty `pickLockStr` is a permanently-locked / "never open"
chest. The original blocks the player and emits `PLAYER_MOB_NEVER_OPEN`.

(Confirmed against `oCMobLockable::Interact` @`0x00723cf0`, which only clears the locked
bit when the player actually owns the key — so a no-key/no-pick locked container can never
be opened by the player.)

## OpenGothic file:line

`game/world/objects/interactive.cpp:687` — `Interactive::checkUseConditions` (reached via
`Interactive::canOpen` @ `interactive.h:77`, called from `PlayerControl::interact`
@ `game/game/playercontrol.cpp:330`).

## Divergence

In `checkUseConditions`, for the player the only refusal branches are key-set, pick-set, or
key+pick (lines 709-720). When `keyInst==size_t(-1)` (empty `keyInstance`) **and**
`needToPicklock==false` (empty `pickLockStr`), none of the early `return true` paths fire and
none of the refusal branches fire, so control falls through past the `if(isPlayer)` block to
the `conditionFunc`/`useWithItem` checks (lines 728-741) and ultimately `return true`.

Result: a `locked` container with no key requirement and no pick-combination is opened freely
by the player in OpenGothic, whereas the original refuses it (`PLAYER_MOB_NEVER_OPEN`).

Note `isLockCracked` is initialized to `!locked` for containers/doors
(`interactive.cpp:60,68`) and stays `false` for an as-yet-uncracked locked container; for
plain non-lockable mobs `locked` is `false`, so the fix must key off `locked` to avoid
regressing ordinary mobs.

## Proposed patch

Add the missing "never open" refusal to the existing player else-if chain, gated on the
`locked` flag so it cannot affect non-lockable mobs.

`game/world/objects/interactive.cpp` (inside `if(isPlayer)`, extending the chain at line 717-720):

OLD:
```cpp
    else if(needToPicklock) { // lockpick only
      sc.printMobMissingLockpick(npc);
      return false;
      }

    }
```
NEW:
```cpp
    else if(needToPicklock) { // lockpick only
      sc.printMobMissingLockpick(npc);
      return false;
      }
    else if(locked && !isLockCracked) {
      // NOTE: in original-game oCMobLockable::CanOpen (Gothic2.exe @0x007244f0) a locked
      // mob with neither a key-instance nor a pick-combination string is a permanently
      // locked "never open" object: the player is refused and PLAYER_MOB_NEVER_OPEN is
      // emitted. OpenGothic fell through and opened it.
      sc.printMobNeverOpen(npc);
      return false;
      }

    }
```

Supporting new script-message helper, mirroring the existing `printMobMissing*` ones
(verified pattern at `game/game/gamescript.cpp:1014-1023` and `1025-1033`):

`game/game/gamescript.h` (after line 131, `printMobMissingLockpick`):
```cpp
    void  printMobNeverOpen           (Npc &npc);
```

`game/game/gamescript.cpp` (new function alongside the other `printMob*`):
```cpp
void GameScript::printMobNeverOpen(Npc& npc) {
  auto id = vm.find_symbol_by_name("player_mob_never_open");
  if(id==nullptr) {
    if(owner.version().game==1)
      owner.player()->playAnimByName("T_DONTKNOW", BS_NONE);
    return;
    }
  ScopeVar self(*vm.global_self(), npc.handlePtr());
  vm.call_function<void>(id);
  }
```

Grep-verified OG symbols: `locked` (interactive.h:171), `isLockCracked` (interactive.h:181),
`sc`=`world.script()` (interactive.cpp:690), `GameScript::find_symbol_by_name`/`vm`/`ScopeVar`
pattern (gamescript.cpp:1014). `printMobNeverOpen` and the `player_mob_never_open` symbol are
new (the script symbol matches the original's `PLAYER_MOB_NEVER_OPEN`).
