# AI-state init does not reset perception interval to 5000 ms

Confidence: Medium-High

## Original function + address
`oCNpc_States::DoAIState` (Gothic2.exe `0x0076d1a0`), driven once per frame from
`oCAIHuman::DoAI` (`0x0069bab0`).

`DoAIState` runs the script state machine. The state-execution phase is stored in
the field at `this+0x30` (-1 = INIT, 0 = LOOP, 1 = END-pending, 2 = ENDED). In the
INIT branch (`phase == -1`) the original does, in order:

1. `oCNpc::SetPerceptionTime(npc, 5000.0)` (`0x0075dba0`) — reset the perception
   interval to the 5000 ms default;
2. if `funcIni > 0`, `CallFunc(funcIni)` — run the state's `_INI` function;
3. set `phase = 0` (advance to LOOP).

So the perception interval is reset to 5000 ms **before** the state's `_INI`
function runs. If `_INI` calls `Npc_SetPercTime`, that override wins for the new
state; if it does not, the new state assesses on the 5 s default cadence regardless
of whatever (possibly much shorter) interval the *previous* state had installed.
`SetPerceptionTime` itself only shrinks the already-armed pending wait
(`remaining mod new-interval`); it never extends it, so the reset takes effect from
the next scan onward.

## OpenGothic file:line
`game/world/objects/npc.cpp:3066-3074` (`Npc::tickRoutine`, the
`if(!aiState.started)` init branch — the mirror of the original INIT phase):

```cpp
  if(!aiState.started) {
    aiState.started      = true;
    aiState.loopNextTime = owner.tickCount();
    // WA: for gothic1 dialogs
    perceptionNextTime   = owner.tickCount();
    sc.invokeState(this,currentOther,currentVictim,aiState.funcIni);
    return;
    }
```

`Npc::setPerceptionTime` is only ever invoked once, from the constructor
(`npc.cpp:200`). Neither `startState` (`npc.cpp:2958`) nor this init branch resets
`perceptionTime`.

## Divergence
The original resets the perception interval to 5000 ms at the start of *every* AI
state, just before the state's `_INI` function runs. OpenGothic only sets it once at
construction and then carries whatever value the last `Npc_SetPercTime` left in
place across state transitions. Concretely: a state whose `_INI` lowers the perc
time (e.g. an alert/combat assess state calling `Npc_SetPercTime(self, 0.x)`) leaves
that fast cadence installed; when the NPC subsequently enters a routine/idle state
whose `_INI` does *not* call `Npc_SetPercTime`, the original snaps back to the 5 s
default while OpenGothic keeps scanning on the old fast interval — making idle NPCs
perceive far more aggressively (and at higher CPU cost) than in the original. This is
distinct from the already-fixed constructor default (`aistate-perctime-default.md`),
which only affects NPCs that have *never* had a perc time set.

## Proposed patch
`game/world/objects/npc.cpp`, in `Npc::tickRoutine` init branch.

OLD:
```cpp
  if(!aiState.started) {
    aiState.started      = true;
    aiState.loopNextTime = owner.tickCount();
    // WA: for gothic1 dialogs
    perceptionNextTime   = owner.tickCount();
    sc.invokeState(this,currentOther,currentVictim,aiState.funcIni);
    return;
    }
```
NEW:
```cpp
  if(!aiState.started) {
    aiState.started      = true;
    aiState.loopNextTime = owner.tickCount();
    // WA: for gothic1 dialogs
    perceptionNextTime   = owner.tickCount();
    // NOTE: in original-game oCNpc_States::DoAIState @0x0076d1a0 the INIT phase
    // (state-phase field this+0x30 == -1) calls oCNpc::SetPerceptionTime(self,5000.0)
    // @0x0075dba0 *before* invoking the state's _INI function, resetting the
    // perception interval to the 5s default at every state start (an _INI that calls
    // Npc_SetPercTime then overrides it). Without this, a fast perc time installed by a
    // previous state leaks into idle/routine states whose _INI never re-sets it.
    setPerceptionTime(5000);
    sc.invokeState(this,currentOther,currentVictim,aiState.funcIni);
    return;
    }
```

`setPerceptionTime(5000)` is the exact mirror of the original
`SetPerceptionTime(5000.0)` (it already implements the pending-wait shrink/clamp).
Placing it before `invokeState(...funcIni)` preserves the original ordering so an
`_INI` that calls `Npc_SetPercTime` still wins.

Residual uncertainty (why Medium-High not High): the reset lives in `DoAIState`'s
INIT phase, whose exact entry condition relative to OpenGothic's `aiState.started`
flag was inferred from the phase-field (`this+0x30`) control flow rather than from a
labelled symbol; the 5000.0 constant and the `SetPerceptionTime` call site in the
INIT branch are directly visible in the decompilation.
