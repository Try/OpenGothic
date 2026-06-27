# Npc_IsInRoutine reports false when the routine slot is interrupted by an AI state

**Confidence:** High

## Original function + address

- `Npc_IsInRoutine` external — `FUN_006e51c0` @ `0x006e51c0` (oGameExternal.cpp).
- `oCNpc_States::GetLastRoutineState` @ `0x0076e890` (oNpcStates.cpp).

The script external `Npc_IsInRoutine(npc, state)` resolves the NPC, then evaluates
exactly one comparison: it calls `GetLastRoutineState()` and returns whether that
value equals the queried `state`. `GetLastRoutineState()` reads the NPC's *currently
active daily-routine entry* (the routine slot pointer `oCNpc_States+0xa4`, updated by
`SetRoutine`/`UpdateSingleRoutine` as game-time crosses TA boundaries) and returns
that entry's `oCRtnEntry::GetState()`. If there is no current entry it returns `0`.

Crucially, the result depends ONLY on the routine *schedule* for the current time. It
does NOT consult what state the NPC is actually executing. So while an NPC whose
current routine slot is, say, `ZS_GUARD` is temporarily running a non-routine state
(combat `ZS_ATTACK`, dialog `ZS_TALK`, a perception-pushed state, flee, etc.),
`Npc_IsInRoutine(npc, ZS_GUARD)` still returns true in the original game.

## OpenGothic file:line

`game/world/objects/npc.cpp:4498`

```cpp
bool Npc::isInRoutine(ScriptFn stateFn) const {
  auto& rout = currentRoutine();
  return rout.callback==stateFn && aiState.funcIni==stateFn;
  }
```

`currentRoutine().callback` is the faithful equivalent of `GetLastRoutineState()` (the
state function of the routine slot for the current time). But OpenGothic ANDs in a
second clause, `aiState.funcIni==stateFn`, which additionally requires the NPC to be
*actively executing* that exact state right now.

## Divergence

When the NPC's routine schedule slot matches `stateFn` but the NPC is currently in a
different, non-routine AI state (in combat, in dialog, mid-perception, fleeing, etc.),
the active `aiState.funcIni` is that other state's function, so the extra clause makes
`Npc_IsInRoutine` return **false** — whereas the original returns **true** because it
only inspects the routine schedule. This breaks the common scripting idiom of gating
behavior on "is this NPC scheduled to be doing X right now" (e.g.
`if(Npc_IsInRoutine(self, ZS_GUARD))`), which in the original keeps reporting the
scheduled activity even while the NPC is briefly interrupted.

## Proposed patch

```cpp
// OLD
bool Npc::isInRoutine(ScriptFn stateFn) const {
  auto& rout = currentRoutine();
  return rout.callback==stateFn && aiState.funcIni==stateFn;
  }

// NEW
bool Npc::isInRoutine(ScriptFn stateFn) const {
  // NOTE: in original-game Npc_IsInRoutine @0x006e51c0 the result is purely
  // GetLastRoutineState()==state @0x0076e890, i.e. it inspects the active routine
  // slot's state for the current time and does NOT require the NPC to be executing
  // that state; an interrupting AI state (combat/dialog/perception) must not flip
  // the answer to false.
  auto& rout = currentRoutine();
  return rout.callback==stateFn;
  }
```

Grep-verified OG symbols: `Npc::isInRoutine(ScriptFn)` and `Npc::currentRoutine()`
exist (`game/world/objects/npc.cpp:4498`, `:3377`); `Routine::callback` is a `ScriptFn`
(`game/world/objects/npc.h:428`). The change only drops the extra
`&& aiState.funcIni==stateFn` conjunct, so it is surgical and build-safe.
