# ZS state lifecycle: spurious final `_Loop` on the eTime / routine-boundary tick

**Confidence:** Low-Medium (analysis is well-grounded in the original decompile; observable in-game impact is small because the extra `_Loop`'s side effects are usually masked by the immediately-following `clearState`/`clearAiQueue`).

## Original function + address (prose)

`oCNpc_States::DoAIState` @ `0x0076d1a0` is the per-tick state pump. Its phase
machine uses a small set of fields on the states object:

- `+0x1c/+0x20/+0x24` = current `funcIni / funcLoop / funcEnd`,
- `+0x30` = phase (`-1` = run Ini, `0` = run Loop, `1` = run End, `2` = done),
- `+0x34` = active flag.

Crucially, the routine-progression decision happens at the **top** of
`DoAIState`, *before* the phase switch. When the current daily-routine slice is
no longer the active one (the `oCNpc` "is-current-routine" virtual at vtable
`+0x104` returns 0), `DoAIState` calls `oCNpc_States::ActivateRtnState`
@ `0x0076c330` with `param_1 = 0`. `ActivateRtnState` in turn calls
`StartAIState(newRoutineState, param_2 = 1, ...)`, and `StartAIState`
@ `0x0076c840` for `param_2 != 0` sets the **current** state's phase
(`+0x30`) to `1`. Therefore, on the very tick the routine boundary is crossed,
the phase switch that runs later in the same `DoAIState` call sees phase `== 1`
and executes the outgoing state's `funcEnd` — it does **not** call the outgoing
state's `funcLoop` that tick. The outgoing `_Loop` is skipped on the boundary
tick; only `_End` runs, then the staged new routine state is promoted next tick.

In short: the original never runs a state's `_Loop` on the tick it decides to
end/replace that state due to its time window closing.

## OG file:line

`/Users/admin/Downloads/opengothic/game/world/objects/npc.cpp:3206-3238`
(`Npc::tickRoutine`).

## Divergence

OpenGothic models routine progression with `aiState.eTime` (set to the routine
slice end via `startState(..., endTime(r), ...)`) and an expiry check **after**
running `funcLoop`:

```cpp
if(aiState.funcLoop.isValid()) {
  ...
  loop = sc.invokeState(this,currentOther,currentVictim,aiState.funcLoop);  // runs every tick
  }
...
if(aiState.eTime<=owner.time()) {                 // checked AFTER the loop call
  if(currentTarget==nullptr && outputPipe->isFinished())
    loop = LOOP_END;
  }
if(loop!=LOOP_CONTINUE) {
  clearState(false);                              // runs funcEnd
  ...
  }
```

So on the tick where `eTime` has elapsed, OG executes the outgoing state's
`funcLoop` one final time and *then* forces `LOOP_END`. The original executes
only `funcEnd` on that tick (the `_Loop` is skipped, because the state's phase
was already advanced to `1` at the top of `DoAIState`). OpenGothic therefore
emits one extra `ZS_<state>_Loop` invocation per routine transition compared to
`Gothic2.exe`.

Impact is usually small: the extra `_Loop` runs immediately before
`clearState(false)` + `clearAiQueue()`, so any AI action it enqueues
(`AI_PlayAni`, `AI_Output`, …) is dropped, and most routine `_Loop` bodies are
idempotent. It becomes observable only when a routine `_Loop` has an immediate
non-queued side effect (state variable mutation, perception toggle, attribute
change) on the very tick the routine window closes.

## Proposed patch (OLD/NEW)

Evaluate the time-expiry *before* invoking `funcLoop`, so the outgoing state's
`_Loop` is skipped on the boundary tick (matching the original, where the phase
is advanced to End at the top of `DoAIState` before the loop body would run).
The combat / dialog guard (`currentTarget==nullptr && outputPipe->isFinished()`)
is preserved unchanged, so states that must not be interrupted still run their
`_Loop` normally.

```cpp
// OLD (npc.cpp ~3208-3231)
    int loop = LOOP_CONTINUE;
    if(aiState.funcLoop.isValid()) {
      static const float MAX_DIST = 300;
      if(fastPath && currentFp!=nullptr && qDistTo(currentFp) < MAX_DIST*MAX_DIST) {
        loop = LOOP_CONTINUE;
        }
      else if(fastPath && currentFp!=nullptr) {
        // for debugging
        loop = sc.invokeState(this,currentOther,currentVictim,aiState.funcLoop);
        }
      else {
        loop = sc.invokeState(this,currentOther,currentVictim,aiState.funcLoop);
        }
      } else {
      // ZS_DEATH   have no loop-function, in G1, G2-classic
      // ZS_GETMEAT have no loop-function, in G2-notr
      loop = owner.version().hasZSStateLoop() ? 1 : 0;
      }

    if(aiState.eTime<=owner.time()) {
      // Avoid interruption of ZS_TALK/ZS_ATTACK
      if(currentTarget==nullptr && outputPipe->isFinished())
        loop = LOOP_END;
      }
```

```cpp
// NEW
    int loop = LOOP_CONTINUE;
    // NOTE: in original-game oCNpc_States::DoAIState @0x0076d1a0 the routine-window
    // expiry is decided at the top of the tick (ActivateRtnState @0x0076c330 ->
    // StartAIState @0x0076c840 sets the current state's phase to "End"), so the
    // outgoing state's _Loop is NOT run on the boundary tick: only _End runs.
    // Mirror that by forcing LOOP_END before invoking funcLoop on the expiry tick.
    const bool timeExpired = aiState.eTime<=owner.time() &&
                             currentTarget==nullptr && outputPipe->isFinished();
    if(timeExpired) {
      loop = LOOP_END;
      }
    else if(aiState.funcLoop.isValid()) {
      static const float MAX_DIST = 300;
      if(fastPath && currentFp!=nullptr && qDistTo(currentFp) < MAX_DIST*MAX_DIST) {
        loop = LOOP_CONTINUE;
        }
      else if(fastPath && currentFp!=nullptr) {
        // for debugging
        loop = sc.invokeState(this,currentOther,currentVictim,aiState.funcLoop);
        }
      else {
        loop = sc.invokeState(this,currentOther,currentVictim,aiState.funcLoop);
        }
      } else {
      // ZS_DEATH   have no loop-function, in G1, G2-classic
      // ZS_GETMEAT have no loop-function, in G2-notr
      loop = owner.version().hasZSStateLoop() ? 1 : 0;
      }
```

**Status: DEFERRED for landing** — the behavioral payoff is small (the extra
`_Loop` is normally masked by the subsequent `clearState`/`clearAiQueue`), while
reordering the core state pump carries non-trivial regression risk and would
want in-game verification on routine transitions (cook/smith/pray/sleep states)
before merging. Recorded here as a genuine, original-verified divergence in the
ZS state-end / eTime-expiry ordering.
