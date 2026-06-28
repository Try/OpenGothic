# ZS_-prefix gate: non-"ZS_" AI_StartState target should be a one-shot call, not a looping state

**Confidence:** Medium
(Engine-logic divergence is certain and well-understood; real-world manifestation is limited to
content that passes a non-`ZS_`-named function to `AI_StartState`, which is unusual in vanilla but
legal and used by some mods.)

## Original function + address

`oCNpc_States::StartAIState(int func, int statebehaviour, ...)` at **Gothic2.exe 0x0076c840**
(the `int`-index overload; the `zSTRING`-name overload at `0x0076c700` resolves the name and forwards
to it). This is the single chokepoint that both `ActivateRtnState` (daily routine /
`start_aistate`, 0x0076c330) and the script-facing `AI_StartState` external funnel through.

In the `func >= 0` branch the engine fetches the symbol *name* via `zCParser::GetSymbolInfo` and runs

```
pos = name.Search(0, "ZS_", 1);   // index of "ZS_" within the function name
if (pos != 0) {
    // name does NOT begin with "ZS_": set SELF, CallFunc(func) exactly once, return 1.
    // The function is invoked as a one-shot; NO funcIni/funcLoop/funcEnd state is installed,
    // no eTime/loop bookkeeping is created, the daily routine continues unchanged.
}
// else (name begins with "ZS_"): install funcIni=func, funcLoop=name+"_Loop",
// funcEnd=name+"_End" as a real looping AI-state.
```

So the engine *requires* an AI-state function to be named `ZS_*`. A non-`ZS_` function handed to
`AI_StartState` is called once and discarded; it never becomes a looping state and never gets a
`_Loop`/`_End` lifecycle.

## OG file:line

- `game/game/gamescript.cpp:3332` `GameScript::ai_startstate(...)` — pushes `AiQueue::aiStartState(st.funcIni, state, ...)` unconditionally.
- `game/world/objects/npc.cpp:2722` `case AI_StartState:` — processes the queued action by calling `startState(act.func, act.s0, aiState.eTime, act.i0==0)`.
- `game/world/objects/npc.cpp:3134` `Npc::startState(...)` and `game/game/aistate.cpp:5` `AiState::AiState(...)` — unconditionally derive `funcIni/funcLoop/funcEnd` from the symbol name and install a looping state.

## Divergence

OpenGothic installs **every** `AI_StartState` target as a full looping AI-state regardless of its name.
There is no `ZS_`-prefix gate anywhere in `ai_startstate`, the `AI_StartState` queue handler, or
`startState`. Consequently a non-`ZS_`-named function passed to `AI_StartState`:

- original `Gothic2.exe`: invoked exactly once, then the NPC keeps its current routine/state;
- OpenGothic: installed as a perpetual state (its `funcLoop`/`funcEnd` are name-derived — usually
  resolving to `-1`, so the state loops forever via the no-loop-function path and never self-terminates),
  replacing the routine.

This changes observable AI behavior (the NPC gets stuck in a synthesized state instead of running the
one-shot and continuing) for any content that relies on the documented ZenGin rule that AI states are
`ZS_`-prefixed.

## Proposed patch

Gate the state install on the `ZS_` prefix in the `AI_StartState` queue handler (the OG analogue of
`StartAIState`). For a non-`ZS_` target, invoke it once and do not install a state.

`game/world/objects/npc.cpp`, `case AI_StartState:` (around line 2722)

OLD:
```cpp
    case AI_StartState:
      // NOTE: a new state can be stater within a daly routiine, such as TA_Sleep, with: ZS_GotoBed -> ZS_Sleep.
      // In such cases it's important to preserve aiState.eTime.
      if(startState(act.func,act.s0,aiState.eTime,act.i0==0)) {
        setOther(act.target);
        setVictim(act.victim);
        }
      break;
```

NEW:
```cpp
    case AI_StartState: {
      // NOTE: in original-game oCNpc_States::StartAIState @0x0076c840 a state function is installed as a
      // looping AI-state only when its name begins with "ZS_" (name.Search(0,"ZS_",1)==0); a non-"ZS_"
      // function is invoked exactly once (one-shot) and the current routine/state is left untouched.
      const auto* sym   = owner.script().findSymbol(act.func.ptr);
      const bool  isZS  = (sym!=nullptr && sym->name().rfind("ZS_",0)==0);
      if(!isZS) {
        owner.script().invokeState(this,currentOther,currentVictim,act.func);
        break;
        }
      // NOTE: a new state can be stater within a daly routiine, such as TA_Sleep, with: ZS_GotoBed -> ZS_Sleep.
      // In such cases it's important to preserve aiState.eTime.
      if(startState(act.func,act.s0,aiState.eTime,act.i0==0)) {
        setOther(act.target);
        setVictim(act.victim);
        }
      break;
      }
```

Symbols verified to exist: `GameScript::findSymbol(size_t)` (gamescript.h:112), `ScriptFn::ptr`
(gamescript.h:36), `DaedalusSymbol::name()` (used at npc.cpp:370 / 3220),
`GameScript::invokeState(Npc*,Npc*,Npc*,ScriptFn)` (gamescript.h:136), `currentOther` / `currentVictim`.

Caveat (the reason for Medium, not High): the original one-shot path leaves `OTHER`/`VICTIM` as the
leftover global parser instances rather than the action's target/victim; the patch approximates this by
passing the NPC's current other/victim and deliberately does **not** call `setOther/setVictim` for the
one-shot. If the rarity/uncertainty is deemed too high to touch, treat as **DEFERRED** — the divergence
is real but does not manifest on `ZS_`-named states, which is all of vanilla's `AI_StartState` usage.
