# Lock-pick: spurious G_PickLock(success) fired on the combination-complete keypress

**Confidence:** Medium-High
(The call-vs-no-call divergence is certain from the decompile of the completion branch; the
magnitude of the in-game effect depends on what the Daedalus `G_PickLock` does on a "success"
event — at minimum an extra success sound, and an extra success event if the script awards
talent/XP per correct pick.)

## Original function + address (prose)

`oCMobLockable::Interact` (Gothic2.exe @0x00723cf0) drives one lock-pick keypress. The
combination index lives on the mob (`mobLockable+0x234`, the index being that field `>> 2`,
bit 0 being the "pick-in-progress" flag) and the pick-string length is at `mobLockable+0x258`.

For an L/R press it calls the virtual `oCMobLockable::PickLock` (@0x00724800), which compares
the current combination character to the pressed key, advances the index on a match (or resets
it to 0 on a mismatch), and returns 1 (correct) or 0 (wrong). Back in `Interact` the original
then branches on three, and only three, outcomes, each of which calls the Daedalus `G_PickLock`
(via `zCParser::CallFunc`) with a distinct argument pair:

- correct press, index still `< length`  -> `G_PickLock(bSuccess=1, bBrokenOpen=0)`
- wrong press, lockpick survives the roll  -> `G_PickLock(0, 0)`
- wrong press, lockpick breaks on the roll -> `G_PickLock(0, 1)`

Crucially, the **combination-complete** case (correct press that pushes the index to
`length`) takes a *separate* branch: it clears the in-progress flag
(`field & 0xFFFFFFFE`) and posts an `oCMobMsg` open/unlock message to the event manager, then
returns. It does **not** call `G_PickLock` at all, and the original never passes the
`(bSuccess=1, bBrokenOpen=1)` argument pair anywhere. The break roll itself
(`DEX < rand()%100 + 1`, attribute index 5 = `ATR_DEXTERITY`) already matches OpenGothic's
`dex <= script.rand(100)` and is not the issue here.

## OpenGothic file:line

`game/game/playercontrol.cpp:1045-1054` (`PlayerControl::processPickLock`)

## Divergence

On the winning keypress OpenGothic executes:

```cpp
if(prog>=cmp.size()) {
  script.invokePickLock(pl,1,1);   // <-- extra G_PickLock(success=1) call
  inter.setAsCracked(true);
  prog = 0;
  }
```

The original fires **no** `G_PickLock` callback on the completing press — it simply marks the
lock open. OpenGothic instead invokes `G_PickLock(1,1)`, a "success" event the original raises
for *every step except the last*, plus a `(1,1)` argument combination the original never uses.
The result is one spurious pick-success event per fully-picked lock: an extra success sound on
the unlock press, and (if the script's `G_PickLock` grants talent progress/XP on success) an
extra reward not present in the original.

## Proposed patch

Drop the success callback on completion so the winning press only marks the lock cracked,
matching the original's complete-branch (which posts the open message and never calls
`G_PickLock`). The intermediate-step call `invokePickLock(pl,1,0)` is left untouched.

OLD (`game/game/playercontrol.cpp`):
```cpp
    prog++;
    if(prog>=cmp.size()) {
      script.invokePickLock(pl,1,1);
      inter.setAsCracked(true);
      prog = 0;
      } else {
      script.invokePickLock(pl,1,0);
      }
```

NEW:
```cpp
    prog++;
    if(prog>=cmp.size()) {
      // NOTE: in original-game oCMobLockable::Interact @0x00723cf0 the combination-complete
      // branch only clears the in-progress flag and posts the unlock message -- it does NOT
      // call G_PickLock. Calling it here raised an extra pick-success event (sound / possible
      // talent reward) per cracked lock. Only mark the lock cracked.
      inter.setAsCracked(true);
      prog = 0;
      } else {
      script.invokePickLock(pl,1,0);
      }
```

Grep-verified OG symbols: `GameScript::invokePickLock` (`game/game/gamescript.cpp:1218`),
`Interactive::setAsCracked` (`game/world/objects/interactive.h:67`),
`Interactive::setLockProgress`/`lockProgress` (`interactive.h:69-70`),
`Interactive::pickLockCode` (`interactive.h:66`).
