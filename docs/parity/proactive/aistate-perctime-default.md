# Default perception interval: 5000 ms (original) vs ~1 ms (OG)

Confidence: Medium

## Original fn + addr
`oCNpc::oCNpc` constructor (0x0072d950); field `this+0x904` is the perception
interval, `this+0x900` the fractional accumulator, both consumed by
`oCNpc::PerceptionCheck` (0x0075dd30) and updated by `oCNpc::SetPerceptionTime`
(0x0075dba0).

The constructor zero-inits the accumulator (`this+0x900 = 0`) and then sets the
perception interval `this+0x904` to the float constant `0x459c4000`, which is
`5000.0`. So until the script calls `Npc_SetPercTime`, an NPC re-runs its periodic
perception scan only once every 5000 ms of game time. `PerceptionCheck` early-returns
while the accumulator is below this interval.

## OG file:line
`game/world/objects/npc.h:573` and `game/world/objects/npc.cpp:4191-4192`.

```
uint64_t perceptionTime = 0;                       // npc.h:573
uint64_t Npc::perceptionTimeClampt() const {       // npc.cpp:4191
  return std::max<uint64_t>(perceptionTime, 1);
  }
```

`perceptionNextTime` is re-armed as `tickCount + perceptionTimeClampt()`
(npc.cpp:4223, 4253), and the world tick fires `perceptionProcess` whenever
`percNextTime() <= tickCount` (worldobjects.cpp:250-252). With the default of 0,
`perceptionTimeClampt()` returns 1, so an NPC whose script has not yet called
`Npc_SetPercTime` runs the perception scan essentially every tick (~1 ms) instead
of every 5000 ms.

## Divergence
Default perception cadence before the first `Npc_SetPercTime` is 5000 ms in the
original and ~1 ms in OpenGothic — a 5000x faster default scan. Gameplay-different:
freshly created NPCs/monsters (and any never assigned a perc time) assess the player
/enemies far more aggressively and far more often than in the original until a script
overrides it, and the periodic scan cost is much higher.

## Proposed patch
`game/world/objects/npc.h`

OLD:
```
    uint64_t                       perceptionTime    =0;
```
NEW:
```
    // NOTE: in original-game oCNpc ctor sets the perception interval to 5000ms
    // (float 5000.0); NPCs scan every 5s until Npc_SetPercTime overrides it.
    uint64_t                       perceptionTime    =5000;
```

(`perceptionTimeClampt`'s min-1 guard can stay; it only matters if a script ever sets
0 explicitly.)
