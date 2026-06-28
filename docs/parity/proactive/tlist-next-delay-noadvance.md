# zCTriggerList NEXT mode: actTarget advances even when a per-target fire-delay is set

**Confidence:** Medium (binary logic is unambiguous/High; real-world exposure requires a `NEXT`-mode list whose targets carry a non-zero `delay`, which I could not confirm exists in shipping G2 data)

## Original fn + address

`zCTriggerList::DoTriggering` (Gothic2.exe @0x00615190) drives all three list modes off
`field_0x1f8` (process mode: 0=ALL, 1=NEXT/FIFO, 2=RANDOM) and the shared list cursor
`field_0x1fc` (actTarget). In the **NEXT** branch (`field_0x1f8 == 1`) it first skips empty
slots, then branches on the selected target's per-target fire-delay `field_0x1e0[actTarget]`:

- **delay <= 0:** it calls `zCTriggerList::TriggerActTarget` @0x00614f30 immediately, then
  advances the cursor `actTarget = (actTarget + 1) % 6`, and finishes (SetSleeping path).
- **delay > 0:** it stores the activator and arms a vob timer via `zCVob::SetOnTimer(delay*1000)`
  and returns **without advancing actTarget**. When the timer expires, `zCTriggerList::OnTimer`
  @0x00615100 fires `TriggerActTarget` for the *current* `actTarget` and then advances the cursor
  **only when `field_0x1f8 == 0` (ALL mode)**; for NEXT it goes straight to the SetSleeping/finish
  tail and never touches `actTarget`.

Net result in the original: a NEXT-mode list with a non-zero per-target delay leaves `actTarget`
pinned, so every trigger re-fires the *same* target rather than walking down the list. (The cursor
only ever advances on the zero-delay NEXT path.) RANDOM advances/excludes via the re-roll; ALL
advances inside OnTimer. Only NEXT couples the advance to a zero delay.

## OG file:line

`/Users/admin/Downloads/opengothic/game/world/triggers/triggerlist.cpp:45-53` (the
`zenkit::TriggerBatchMode::NEXT` case in `TriggerList::emitList`).

## Divergence

OpenGothic advances the cursor unconditionally:

```cpp
auto& i = targets[next];
next = (next+1)%uint32_t(targets.size());   // always advances
uint64_t time = world.tickCount()+uint64_t(i.delay*1000);
...
```

So for a NEXT list whose targets have a positive `delay`, OpenGothic walks 0,1,2,... while the
original re-fires target 0 forever. For the common zero-delay NEXT list the sequences are identical
(both advance once per fire), so this only manifests when per-target delays are present.

## Proposed patch

OLD (`triggerlist.cpp`, NEXT case):
```cpp
    case zenkit::TriggerBatchMode::NEXT: {
      auto& i = targets[next];
      next = (next+1)%uint32_t(targets.size());

      uint64_t time = world.tickCount()+uint64_t(i.delay*1000);
      TriggerEvent ex(i.name,vobName,time,type);
      world.execTriggerEvent(ex);
      break;
      }
```

NEW:
```cpp
    case zenkit::TriggerBatchMode::NEXT: {
      // NOTE: in original-game zCTriggerList::DoTriggering @0x00615190 the NEXT cursor (field_0x1fc)
      // is advanced only on the zero-delay fire path; when the selected target carries a positive
      // per-target fire-delay it is armed via zCVob::SetOnTimer and zCTriggerList::OnTimer
      // @0x00615100 fires it WITHOUT advancing the cursor (the advance there is gated on ALL mode).
      // So a NEXT list with per-target delays re-fires the same target every trigger; OpenGothic
      // advanced unconditionally and walked the list.
      auto& i = targets[next];
      uint64_t time = world.tickCount()+uint64_t(i.delay*1000);
      if(i.delay<=0)
        next = (next+1)%uint32_t(targets.size());

      TriggerEvent ex(i.name,vobName,time,type);
      world.execTriggerEvent(ex);
      break;
      }
```

Verified OG symbols exist: `targets`, `next`, `i.delay`, `i.name`, `vobName`,
`world.tickCount()`, `world.execTriggerEvent` (triggerlist.cpp / triggerlist.h).
