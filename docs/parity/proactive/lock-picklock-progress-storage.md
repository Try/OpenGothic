# Pick-lock combination progress is per-player and reset on detach, not per-mob

**Confidence:** High (root cause certain; fix shape clear but spans Interactive state + serialization, so a minimal variant is proposed and a fuller variant is noted)

## Original function + address (prose only)

`oCMobLockable::PickLock` (Gothic2.exe @ 0x00724800) and its caller
`oCMobLockable::Unlock` (@ 0x00724a70) implement the L/R combination game.

The combination *progress index* is stored **on the lockable mob itself**, packed into
the lockable's state dword at object offset 0x234: bit0 = locked flag, bit1 = "unlocked"
flag, and the remaining high bits (`>> 2`) are the current combination index. The combo
string and its length live at offsets 0x250 / 0x258 (set by `SetPickLockStr` @ 0x00719780).

In `PickLock`, each keypress reads `progress = (state >> 2)`, compares
`pickLockStr[progress]` against the input char ('L' = 0x4C or 'R' = 0x52, passed from
`Unlock`), and then:
- on a **match**: `state = (state & ~3) + 4 ^ (state & 3)` — i.e. it increments the index
  by one (adds `1 << 2`) while preserving the two low flag bits;
- on a **mismatch**: `state = state & 3` — i.e. it resets the index to 0, again preserving
  the flag bits.

Critically, the index is **only ever reset on a wrong keypress**. Nothing in `PickLock`,
`Unlock`, `Interact`, or the detach path clears it. Because it lives on the mob, partial
progress on a given door/chest survives the player walking away, being interrupted/attacked,
opening the inventory, or otherwise detaching and re-approaching the *same* lock. Each
distinct lockable also keeps its own independent progress.

## OpenGothic file:line

`game/game/playercontrol.cpp:990` (`PlayerControl::processPickLock`), backing field
`game/game/playercontrol.h:149` (`size_t pickLockProgress = 0;`), reset in
`game/game/playercontrol.cpp:1048` (`PlayerControl::quitPicklock`, which zeroes
`pickLockProgress`).

## Divergence

OpenGothic keeps the combination index in a single `size_t pickLockProgress` member on
**`PlayerControl`**, not on the lockable. Consequences vs. the original:

1. **Reset on detach.** `quitPicklock()` sets `pickLockProgress = 0`, and it is called on
   the Back key (playercontrol.cpp:1001) and on lockpick exhaustion (playercontrol.cpp:1022).
   Any detach from the lock therefore restarts the combination from index 0 next time. In the
   original the index persists on the mob and is cleared *only* by entering a wrong direction.
   A player who cracks 3 of 5 tumblers, gets interrupted, and returns must restart from 0 in
   OG but resumes at 3 in vanilla.

2. **Shared across different locks.** Because the counter is global to the player, switching
   from a partially-cracked lock A to lock B carries A's index onto B (until a wrong press or
   a quit resets it). In the original, A and B each carry their own index in their own state
   dword, so they never interfere.

Both behaviours are reachable with vanilla data (any multi-step `pick_string`, e.g. several
chests/doors in the game) and change the felt difficulty and correctness of lock-picking.

## Proposed patch

Move the progress counter onto the `Interactive` (the lockable), matching the original's
per-mob storage, and stop resetting it on detach. `Interactive` already owns all the other
lockable state (`pickLockStr`, `isLockCracked`, grep-verified in
`game/world/objects/interactive.h:163` and `:171`), so this is the natural home.

Minimal change set (symbols below are grep-verified to exist):

In `game/world/objects/interactive.h` (next to the lockable fields ~`:171`):
```
OLD:
    bool                isLockCracked = false;
NEW:
    bool                isLockCracked = false;
    // NOTE: in original-game oCMobLockable::PickLock (Gothic2.exe @0x00724800) the
    // combination index is stored on the mob (state dword @0x234 >> 2) and is reset to 0
    // ONLY on a wrong keypress -- never on detach -- so partial progress persists per-lock.
    uint32_t            pickLockProgress = 0;
```

Add accessors on `Interactive` (header, near `pickLockCode()`/`isCracked()` ~`:66`):
```
NEW:
    uint32_t            lockProgress() const           { return pickLockProgress; }
    void                setLockProgress(uint32_t p)    { pickLockProgress = p; }
```

In `game/game/playercontrol.cpp`, `processPickLock` reads/writes `inter.lockProgress()` /
`inter.setLockProgress(...)` instead of the `PlayerControl::pickLockProgress` member, and the
`quitPicklock(...)` reset at `:1050` is dropped (do NOT zero progress on detach — only the
wrong-keypress branch at `:1016` may reset it; the successful-crack branch at `:1033` also
legitimately resets it). Then delete `pickLockProgress` from `playercontrol.h:149`.

For full parity (recommended, since vanilla persists progress across save/load via the mob's
archived state dword) also serialize the new field in
`Interactive::load`/`Interactive::save` alongside `isLockCracked`
(`game/world/objects/interactive.cpp:117` and `:156`). This part touches the save format and
should bump/guard the serialization version, so it is called out separately.

**Status:** Behavioural root cause is High-confidence. The header field + accessors + reading
it from `processPickLock` is a surgical, build-verifiable fix. The save/load persistence piece
is **DEFERRED** pending a serialization-version bump (format change), but is required for full
"persist across save/load" parity.
