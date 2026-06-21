# zCTriggerList RANDOM mode allows immediate repeats

**Confidence:** Medium

## Original function + address

`zCTriggerList::DoTriggering` at `0x00615190` (Gothic2.exe). The list-process
mode lives at `+0x1f8` (0 = ALL, 1 = NEXT, 2 = RANDOM); the active slot index
is at `+0x1fc`.

In the RANDOM branch (mode == 2) the original re-rolls a fresh index in a loop:
it picks `rand() % 6`, stores it as the new active index, and **repeats the roll
while the new index equals the previous active index** (`while (newIdx == prevIdx)`).
The outer loop additionally re-rolls while the chosen slot has an empty target
name, with a visited-slot guard so an all-empty list bails instead of spinning.
Net effect: RANDOM never fires the same target twice in a row, and it skips
empty slots.

## OpenGothic location

`game/world/triggers/triggerlist.cpp:40-47` (the `TriggerBatchMode::RANDOM` case).

## Divergence

OpenGothic draws `targets[world.script().rand(targets.size())]` with a single
roll and no exclusion of the last-fired index. Consecutive activations of the
same trigger-list can therefore re-fire the identical target back-to-back, which
the original engine explicitly forbids. This is gameplay-observable for RANDOM
trigger-lists driving alternating effects (sounds, animations, oneshot vobs).

The `count`/empty-slot skipping difference is incidental: zenkit's `targets`
vector already contains only real entries, so only the no-immediate-repeat rule
is load-bearing.

## Proposed patch

File: `game/world/triggers/triggerlist.cpp`

OLD:
```cpp
    case zenkit::TriggerBatchMode::RANDOM: {
      auto& i = targets[world.script().rand(uint32_t(targets.size()))];

      uint64_t time = world.tickCount()+uint64_t(i.delay*1000);
      TriggerEvent ex(i.name,vobName,time,TriggerEvent::T_Trigger);
      world.execTriggerEvent(ex);
      break;
      }
```

NEW:
```cpp
    case zenkit::TriggerBatchMode::RANDOM: {
      // NOTE: in original-game zCTriggerList::DoTriggering (0x00615190) the
      // RANDOM mode re-rolls while the new index equals the previously fired
      // one, so the same target is never fired twice in a row.
      uint32_t idx = world.script().rand(uint32_t(targets.size()));
      if(targets.size()>1)
        while(idx==next)
          idx = world.script().rand(uint32_t(targets.size()));
      next = idx;
      auto& i = targets[idx];

      uint64_t time = world.tickCount()+uint64_t(i.delay*1000);
      TriggerEvent ex(i.name,vobName,time,TriggerEvent::T_Trigger);
      world.execTriggerEvent(ex);
      break;
      }
```

(`next` is the already-serialized last-index field reused for the
previous-pick; it is shared with NEXT mode but a vob uses only one mode.)
