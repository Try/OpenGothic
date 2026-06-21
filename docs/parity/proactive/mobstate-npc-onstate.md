# MOBSI on_state callback skipped for non-player NPCs (per-step)

**Confidence:** Medium

## Original function + addr (prose)

`oCMobInter::CheckStateChange` (Gothic2.exe `0x00720440`) is the routine that
commits a pending mobsi state transition. Once the NPC's transition animation
settles and the mob's old state (field `+500`) differs from the pending target
(field `+0x1fc`), it calls the virtual at vtable `+0x110`,
`oCMobInter::SendCallOnStateFunc` (`0x00720ad0`), passing the **new/target
state number**, and only then writes `state = target`.

`SendCallOnStateFunc` queues the callback gated solely on the on-state-func
index field (`+0x1ec`) being non-zero, i.e. on the mobsi actually having an
`on_state_change_function`. The eventual `oCMobInter::CallOnStateFunc`
(`0x00720870`) builds the symbol name `<onStateFunc>_S<state>`, sets `SELF` to
the interacting NPC and `ITEM` to its interact-item, and invokes it.

Crucially: there is **no player-only test** anywhere in this chain. Every
committed state transition fires the on-state callback, for any NPC that drives
the mob (e.g. a routine / `AI_UseMobToState`-scripted NPC operating a lever or
multi-state mechanism).

## OpenGothic location

`game/world/objects/interactive.cpp:411-413` (inside `Interactive::implTick`):

```
  if(npc.isPlayer() && !loopState && attach) {
    invokeStateFunc(npc);
    }
```

The per-step on-state invocation is gated behind `npc.isPlayer()`. The only
other invocations (lines 369-378) fire `invokeStateFunc` exclusively at the two
**terminal** states (reaching `stateNum` while attaching, or reaching `0` while
detaching), and those are not player-gated.

## Divergence

For a non-player NPC stepping a mobsi with `stateNum >= 2` (three or more
states S0..Sn), OpenGothic invokes the on-state callback only at the terminal
state. All **intermediate** `on_state_S1 .. on_state_S{n-1}` callbacks are
skipped, because the per-step call at line 411 is suppressed for NPCs. The
original fires the callback on *every* committed transition regardless of who is
using the mob. Any side effect wired into an intermediate `on_state_S*`
function (trigger emission, world-state change, sound, script flag) therefore
does not happen when an NPC operates the mob, only when the player does.

For the player, removing the gate is behavior-preserving: at the terminal state
lines 370/375 set `loopState=true` first, and line 411 already tests
`!loopState`, so the terminal step will not double-fire.

## Proposed patch

File: `game/world/objects/interactive.cpp`

OLD:
```cpp
  if(npc.isPlayer() && !loopState && attach) {
    invokeStateFunc(npc);
    }
```

NEW:
```cpp
  // NOTE: in original-game oCMobInter::CheckStateChange (Gothic2.exe 0x00720440)
  // fires SendCallOnStateFunc on every committed state transition for ANY npc,
  // not just the player; gated only on the mobsi having an on_state function.
  // Player path is unchanged: at terminal states loopState is already set above,
  // so this !loopState branch will not double-fire.
  if(!loopState && attach) {
    invokeStateFunc(npc);
    }
```
