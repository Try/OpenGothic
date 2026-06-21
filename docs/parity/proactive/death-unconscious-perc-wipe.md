# Death/Unconscious: perception functions wiped on unconscious too

> DEFER: the perception-wipe loop carries an OpenGothic-specific workaround comment ("clear perceptions for William in Jarkentar"). Gating it behind if(death) matches the original (DropUnconscious does not wipe) but may regress that William case; needs confirmation of whether William's scenario is death or unconscious (runtime) before applying.

**Confidence:** Medium

## Original function + address
- `oCNpc::DoDie` (0x736760): on death the engine zeroes the NPC's *active
  perception table* (the per-perception function slots, the `+0x7f4` block of
  66 dwords plus `+0x8fc`). A dead NPC therefore stops reacting to any
  perception. This is intentional and death-specific.
- `oCNpc::DropUnconscious` (0x735eb0): the unconscious path runs a very similar
  prologue (StopTheft, CloseInventory, DropAllInHand, start AI state, fire the
  passive ASSESSDEFEAT perception type 7). Crucially it contains **no**
  perception-table-clearing loop. An unconscious NPC keeps its perception
  function assignments intact, so when it later recovers from ZS_Unconscious it
  perceives the world exactly as before.

The original engine deliberately treats the two cases asymmetrically: clear
perceptions on death, preserve them on knock-out.

## OpenGothic file:line
`game/world/objects/npc.cpp:581` `Npc::onNoHealth(bool death, ...)`, lines
602-603:

```
  // Note: clear perceptions for William in Jarkentar
  for(size_t i=0;i<PERC_Count;++i)
    setPerceptionDisable(PercType(i));
```

`setPerceptionDisable` (npc.cpp:4201) does `perception[t].func = ScriptFn()`,
i.e. it *destroys* the perception's script-function binding (not a temporary
mute).

## Divergence
`onNoHealth` is shared by both the death (`onNoHealth(true,...)`) and the
unconscious (`onNoHealth(false,...)`) paths, and the perception-wipe loop is
**unconditional**. So OpenGothic wipes every perception binding for NPCs that
are merely knocked unconscious, collapsing the original's death/unconscious
asymmetry. After an NPC wakes from ZS_Unconscious in vanilla Gothic its
perceptions are gone (until the mod script happens to re-call Npc_PercEnable),
whereas in the original they survive the knock-out untouched.

## Proposed patch
```
// game/world/objects/npc.cpp  Npc::onNoHealth
  size_t fdead=owner.script().findSymbolIndex(state);
  startState(fdead,"",gtime::endOfTime(),true);
  // Note: clear perceptions for William in Jarkentar
  for(size_t i=0;i<PERC_Count;++i)
    setPerceptionDisable(PercType(i));
```
NEW:
```
  size_t fdead=owner.script().findSymbolIndex(state);
  startState(fdead,"",gtime::endOfTime(),true);
  // NOTE: in original-game oCNpc::DoDie zeroes the active-perception table on
  // death, but oCNpc::DropUnconscious does NOT touch perceptions on knock-out.
  // Only wipe perceptions on actual death so recovered-unconscious NPCs keep
  // perceiving as in the original.
  if(death) {
    // Note: clear perceptions for William in Jarkentar
    for(size_t i=0;i<PERC_Count;++i)
      setPerceptionDisable(PercType(i));
    }
```
