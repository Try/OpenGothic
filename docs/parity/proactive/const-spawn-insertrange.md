# Wrong constant: spawn-manager insert range (1000 vs original 4500)

**Confidence:** High on the value (binary/decompile-verified); Low–Medium on gameplay impact
(the field is currently only exposed to scripts via direct-memory compat, not used by
OpenGothic's own spawn logic — see Caveat).

## Original fn + address (prose)

`oCSpawnManager::GetInsertRange` @ `0x00777830` simply returns the static global
`SPAWN_INSERTRANGE`. OpenGothic itself already identified that global's address as
`9153744` = `0x008BACD0` (see `directmemory.cpp`, constant `SPAWN_INSERTRANGE`, where it
pins OG's own `spawnRange` field onto that exact address so scripts can read/write it).

Reading the literal default straight out of `Gothic2.exe`'s `.data` section at VA
`0x008BACD0` gives the float `4500.0` (`0x458CA000`). The adjacent global at `0x008BACD4`
is `SPAWN_REMOVERANGE` = `5000.0` (`0x459C4000`) — the canonical Gothic spawn-manager pair
(insert at 45 m, remove at 50 m), which corroborates that `0x008BACD0` is indeed the
insert-range field and that `4500` is its real default. The only writer of the global is
the AI console command (`Game_ChangeAIConsole`); nothing else overrides the `.data`
default, so `4500` is the value the engine ships with.

`oCSpawnManager::SetInsertRange` @ `0x00777820` / `SetRemoveRange` @ `0x00777840` confirm
the get/set pair around the same static.

## OG file:line

`game/game/compatibility/directmemory.h:63`

```cpp
float spawnRange = 1000; // 10 meters, for now
```

The self-admitted "for now" comment flags this as a placeholder.

## Divergence

OpenGothic exposes a spawn insert range of `1000` (10 m) to scripts via the
`SPAWN_INSERTRANGE` memory pin. The original `Gothic2.exe` default is `4500` (45 m).
A script/mod that reads `SPAWN_INSERTRANGE` through direct memory (Ikarus/LeGo or raw
`MEM_*` access) therefore sees 1000 instead of 4500 — a 4.5x discrepancy.

## Proposed patch (OLD/NEW)

```cpp
// OLD
    float       spawnRange      = 1000; // 10 meters, for now

// NEW
    // NOTE: in original-game oCSpawnManager::GetInsertRange @0x00777830 returns the static
    // SPAWN_INSERTRANGE (VA 0x008BACD0), whose .data default is 4500.0 (45 m); the adjacent
    // SPAWN_REMOVERANGE @0x008BACD4 is 5000.0. OpenGothic shipped a 1000 placeholder, so
    // scripts reading SPAWN_INSERTRANGE via the directmemory pin saw 10 m instead of 45 m.
    float       spawnRange      = 4500; // 45 meters (original SPAWN_INSERTRANGE default)
```

## Caveat

`spawnRange` is referenced only in `directmemory.cpp` (the `mem32.pin(&spawnRange,
SPAWN_INSERTRANGE, ...)` call) and its declaration here; OpenGothic's NPC spawn/despawn
tiering uses separate hardcoded distances in `worldobjects.cpp` and does not read this
field. So the fix corrects the value scripts observe, not OG's internal spawning. It is a
genuine, fully verified wrong constant and a one-line build-safe change, but its in-game
effect is limited to mods that read `SPAWN_INSERTRANGE` from memory.
