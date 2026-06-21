# Npc_SetPercTime does not shorten the in-flight perception wait

Confidence: Medium

## Original fn + addr
`oCNpc::SetPerceptionTime` (Gothic2.exe 0x0075dba0). The NPC keeps two fields: the
perception interval (`this+0x904`) and a running accumulator/countdown (`this+0x900`)
that `oCNpc::PerceptionCheck` (0x0075dd30) advances each frame and fires the periodic
perception scan when it crosses the interval.

`SetPerceptionTime(newInterval)` stores `newInterval` into the interval field, then
inspects the *current pending accumulator*: if the accumulator is greater than the new
interval, it repeatedly subtracts the new interval from the accumulator until the
accumulator is below the new interval (i.e. it reduces the pending value to
`oldAccumulator mod newInterval`) and writes it back. Effect: lowering an NPC's
perception time takes effect almost immediately for the *current* cycle, not only from
the next cycle onward; the NPC cannot be left waiting out a long stale interval after a
script tightens its perc time.

## OG file:line
`game/world/objects/npc.cpp:4188-4190`

```
void Npc::setPerceptionTime(uint64_t time) {
  perceptionTime = time;
  }
```

OG models the cadence as an absolute deadline `perceptionNextTime` (re-armed to
`tickCount + perceptionTimeClampt()` only when a scan fires, npc.cpp:4224/4254; gated by
`percNextTime() <= tickCount`, worldobjects.cpp:250-252). `setPerceptionTime` changes the
period but leaves the already-armed `perceptionNextTime` untouched.

## Divergence
When a script lowers an NPC's perc time mid-cycle, the original immediately clamps the
remaining wait to under the new interval, so the NPC re-perceives within (at most) the
new interval. In OG the NPC keeps the previously-armed deadline, so it can wait out up to
a full *old* interval before the new (shorter) cadence applies. Gameplay-different for AI
that tightens perc time to react quickly (ambush/alert/dialog setup): the original reacts
this cycle, OG can be one stale interval late.

## Proposed patch
`game/world/objects/npc.cpp`

OLD:
```
void Npc::setPerceptionTime(uint64_t time) {
  perceptionTime = time;
  }
```
NEW:
```
void Npc::setPerceptionTime(uint64_t time) {
  // NOTE: in original-game oCNpc::SetPerceptionTime (0x0075dba0) also reduces the
  // pending perception wait so a freshly-lowered perc time takes effect this cycle:
  // it clamps the remaining wait to (remaining mod new-interval).
  const uint64_t now = owner.tickCount();
  if(time>0 && perceptionNextTime>now) {
    uint64_t remaining = perceptionNextTime - now;
    if(remaining>time)
      perceptionNextTime = now + (remaining % time);
    }
  perceptionTime = time;
  }
```
