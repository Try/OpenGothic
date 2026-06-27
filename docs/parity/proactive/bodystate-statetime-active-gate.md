# Npc_GetStateTime / Npc_SetStateTime ignore the "active script-state" gate

**Confidence:** Medium

## Original function + address (prose)

The script external `Npc_GetStateTime` (external thunk `FUN_006e2560`) calls
`oCNpc::GetAIStateTime` @ `0x0073eff0`, which forwards to
`oCNpc_States::GetStateTime` @ `0x0076c0a0`. That implementation is **guarded**:
it only computes/returns the state time when the state object's "active
script-state" word at `oCNpc_States+0x34` is non-zero; otherwise it returns `0`
unconditionally. Concretely it does `if (states+0x34 != 0) return ftol(*(float*)(states+0x4c)) / 1000; return 0;`.

The script external `Npc_SetStateTime` (external thunk `FUN_006e2640`) calls
`oCNpc_States::SetStateTime` @ `0x0076c0d0`, which is gated the same way: it
writes the state-time float (`*(float*)(states+0x4c) = seconds * 1000.0`) **only
when `states+0x34 != 0`**, and is a no-op otherwise.

The `states+0x34` word is the engine's "a script state is currently loaded"
indicator (routine ZS_* states as well as AI states). It is the same predicate
read by `oCNpc_States::GetState` @ `0x0076c020`, `oCNpc_States::IsInState`
@ `0x0076c040`, `oCNpc_States::IsScriptStateActive` @ `0x0076c080` and
`oCNpc_States::EndCurrentState` @ `0x0076d880`. So in the original, querying or
setting state time on an NPC that has no active script state yields `0` / does
nothing — it never reports elapsed wall-clock time.

## OpenGothic file:line

- `game/game/gamescript.cpp:2418` `GameScript::npc_getstatetime` -> `npc->stateTime()/1000`
- `game/game/gamescript.cpp:2425` `GameScript::npc_setstatetime` -> `npc->setStateTime(val*1000)`
- `game/world/objects/npc.cpp:4601` `Npc::stateTime()`   -> `owner.tickCount() - aiState.sTime`
- `game/world/objects/npc.cpp:4605` `Npc::setStateTime()` -> `aiState.sTime = owner.tickCount() - time`

`Npc::stateTime()` / `Npc::setStateTime()` are called ONLY from these two
externals (grep-verified: no other callers in `game/`).

## Divergence

OpenGothic applies **no "active state" gate**:

- `stateTime()` returns `owner.tickCount() - aiState.sTime`. When the NPC has no
  active script state, `aiState.sTime` is its default/last value (e.g. `0`), so
  `Npc_GetStateTime` returns the entire elapsed world time in seconds instead of
  the original's `0`.
- `setStateTime()` always rewrites `aiState.sTime`, whereas the original
  silently ignores `Npc_SetStateTime` when no script state is active.

The equivalent of the original `states+0x34 != 0` predicate already exists in
OpenGothic as `aiState.funcIni.isValid()` (grep-verified at
`game/world/objects/npc.cpp:3112,3151,3164,4498`; `ScriptFn::isValid()` returns
`ptr!=size_t(-1)` per `game/game/gamescript.h:38`, and `funcIni` defaults to an
invalid `ScriptFn` so a stateless NPC reads as invalid).

This is distinct from the already-handled `bodyStateMasked` masking and from the
deferred stagger `BS_MOD` strip / `BS_SNEAK` walk-mode gates.

## Proposed patch (gate the two leaf accessors, matching the original's
`oCNpc_States::GetStateTime`/`SetStateTime` guards)

`game/world/objects/npc.cpp:4601`

OLD:
```cpp
uint64_t Npc::stateTime() const {
  return owner.tickCount()-aiState.sTime;
  }

void Npc::setStateTime(int64_t time) {
  aiState.sTime = owner.tickCount()-uint64_t(time);
  }
```

NEW:
```cpp
uint64_t Npc::stateTime() const {
  // NOTE: in original-game oCNpc_States::GetStateTime @0x0076c0a0 returns 0
  // unless an active script state is loaded (states+0x34 != 0).
  if(!aiState.funcIni.isValid())
    return 0;
  return owner.tickCount()-aiState.sTime;
  }

void Npc::setStateTime(int64_t time) {
  // NOTE: in original-game oCNpc_States::SetStateTime @0x0076c0d0 is a no-op
  // unless an active script state is loaded (states+0x34 != 0).
  if(!aiState.funcIni.isValid())
    return;
  aiState.sTime = owner.tickCount()-uint64_t(time);
  }
```

Symbols verified to exist: `Npc::aiState` (`AiState`, `npc.h:616`),
`AiState::funcIni` (`ScriptFn`, `npc.h:443`), `ScriptFn::isValid()`
(`gamescript.h:38`), `AiState::sTime` (`npc.h:446`), `Npc::owner.tickCount()`.
Build-safe: both methods are only reached through the two externals, so gating
cannot regress other call sites.
