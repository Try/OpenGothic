# init3 — Fresh-spawn perception fires immediately instead of after one perceptionTime

**Confidence:** High

## Original fn + address

In the original engine the per-NPC perception throttle is a pair of floats inside `oCNpc`:

- offset `0x900` — elapsed-time-since-last-perception **accumulator**
- offset `0x904` — perception **interval** (`perceptionTime`)

`oCNpc::oCNpc` (Gothic2.exe `@0x0072d950`) initializes the accumulator `0x900` to `0` and
the interval `0x904` to `0x459c4000` (= `5000.0f`). The conditional modulo block right after
(`while (interval < accum) accum -= interval`) is a no-op at construction because the
accumulator is `0`. Neither `oCNpc::InitByScript` (`@0x0072ee70`) nor anything else seeds
`0x900` away from `0` for a freshly created NPC.

`oCNpc::SetPerceptionTime` (Gothic2.exe `@0x0075dba0`) pins the meaning of the pair: it writes
the new interval to `0x904` and, when the live accumulator `0x900` exceeds the new interval,
reduces it modulo the interval (`do accum -= interval; while (interval < accum);`). That
reduction only makes sense if the AI tick increments `0x900` by frame time each step and fires a
perception once `0x900 >= 0x904`, then subtracts the interval. Consequently, with the accumulator
starting at `0`, **the first perception of a freshly spawned NPC occurs one full
`perceptionTime` (~5000 ms) after spawn**, not on the first processed frame.

(Field `0x9a4`, which `InitByScript` seeds to `rand()%3000` and `AI_ForceDetection`
`@0x00740730` forces to `100000.0`, is a *separate* detection timer — not the perception pair —
so it is not part of this finding.)

## OG file:line

- `game/world/objects/npc.cpp:579-580` — `perceptionTime=5000`, `perceptionNextTime=0` defaults.
- `game/world/objects/npc.cpp:205` — ctor calls `setPerceptionTime(5000)` but never arms `perceptionNextTime`.
- `game/world/worldobjects.cpp:250-253` — gate: `if(percNextTime<=tickCount()) perceptionProcess(*pl);`

## Divergence

OpenGothic already documents (see the NOTE at `npc.cpp:4537`, citing
`oCNpc::SetPerceptionTime @0x0075dba0`) that it models engine accumulator `0x900` as the
**deadline** `perceptionNextTime` and interval `0x904` as `perceptionTime`. Under that mapping,
"accumulator = 0 at spawn" is equivalent to a deadline of `spawnTime + perceptionTime`. But the
ctor leaves `perceptionNextTime = 0` (its header default), which is a deadline in the past.
`WorldObjects::tickNear` therefore runs `perceptionProcess` on the NPC's very first processed
frame (`0 <= tickCount()` is always true), so every newly spawned/inserted NPC reacts to the
player up to ~5 s earlier than vanilla, and a `Wld_InsertNpc` / world-start batch all perceives
on the same frame. Loaded NPCs are unaffected (`perceptionNextTime` is serialized,
`npc.cpp:220`/`:278`); the divergence is purely a fresh-spawn default. The player is excluded by
`worldobjects.cpp:247`, so arming the deadline for the player instance is harmless.

## Proposed patch

`game/world/objects/npc.cpp`, in the `Npc::Npc(...)` ctor, immediately after `setPerceptionTime(5000);`:

OLD:
```cpp
  setTrueGuild(hnpc->guild); // https://worldofplayers.ru/threads/12446/post-878087
  setPerceptionTime(5000);   // https://github.com/Try/OpenGothic/pull/720#issuecomment-2602908614
  }
```

NEW:
```cpp
  setTrueGuild(hnpc->guild); // https://worldofplayers.ru/threads/12446/post-878087
  setPerceptionTime(5000);   // https://github.com/Try/OpenGothic/pull/720#issuecomment-2602908614
  // NOTE: in original-game oCNpc::oCNpc @0x0072d950 the perception accumulator (engine offset
  // 0x900) starts at 0 and the engine fires perception only once it reaches perceptionTime
  // (offset 0x904, init 5000.0; see oCNpc::SetPerceptionTime @0x0075dba0), so a freshly spawned
  // NPC's first perception is one full perceptionTime after spawn. OpenGothic models 0x900 as the
  // perceptionNextTime deadline, so the equivalent armed deadline is spawnTime + perceptionTime;
  // leaving the header default 0 made every fresh NPC perceive on its first processed frame.
  perceptionNextTime = owner.tickCount() + perceptionTimeClampt();
  }
```

Build-verifiable: `perceptionNextTime` (npc.h:580), `perceptionTimeClampt()` (npc.h:537,
npc.cpp:4550) and `owner.tickCount()` (used throughout npc.cpp) all exist. Must stay after
`setPerceptionTime(5000)` so `perceptionTimeClampt()` reflects the 5000 interval.
