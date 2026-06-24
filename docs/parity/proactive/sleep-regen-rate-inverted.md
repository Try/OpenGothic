# Sleep / natural HP+mana regen: REGENERATEHP/MANA semantics are inverted (points-per-second vs. seconds-per-point)

**Confidence:** High (engine math is unambiguous; the only soft point is that the per-NPC regen timer is wall-clock-quantized in the proposed fix rather than frame-phase-aligned).

## Original function + address

`oCNpc::Regenerate` (Gothic2.exe @ `0x00741fd0`), called once per NPC frame from the
per-NPC update path (the immediate caller at `0x0073e4d1` sits in a function not present in
the symbol dump, i.e. the NPC `DoFrame`/tick routine).

Behaviour (described, not copied):

- The NPC struct holds two floating-point *countdown timers*: one for HP regen, one for mana
  regen. Each frame both timers are decremented by the global per-frame delta (the ZenGin
  frame time, in milliseconds).
- When the HP timer crosses zero AND `attribute[ATR_REGENERATEHP] > 0` AND current HP is
  below HPMAX, the routine adds **exactly +1** HP (via `oCNpc::ChangeAttribute(index=0, +1)`)
  and **reloads the timer to `attribute[ATR_REGENERATEHP] * 1000.0`** (the constant
  `447a0000` = 1000.0f converts the attribute value from *seconds* to *milliseconds*).
- The mana branch is identical: +1 mana when its timer expires, reload to
  `attribute[ATR_REGENERATEMANA] * 1000.0`. (Side note: in the original the mana branch's
  `> 0` enable test reads the *HP* regen field — an original-game quirk — but that is
  tangential to this finding and is not replicated below.)

Net meaning: **`ATR_REGENERATEHP` / `ATR_REGENERATEMANA` express the number of seconds
between each +1 point of regeneration.** This is the same engine path the `ZS_Sleep` daily
routine drives: going to bed raises these regen attributes so the NPC/player heals while
sleeping, so the bug is squarely in the sleep-regen rate.

## OpenGothic file:line

`game/world/objects/npc.cpp:2337` — `Npc::tickRegen`
(invoked from `Npc::tick` at `npc.cpp:2413` and `:2415` with
`hnpc->attribute[ATR_REGENERATEHP]` / `ATR_REGENERATEMANA` as `chg`).

## Divergence

`Npc::tickRegen` integrates `chg` against the wall-clock millisecond phase:

```
int32_t val0 = (time0*chg)/1000;   // time0 = tick % 1000
int32_t val1 = (time1*chg)/1000;   // time1 = time0 + dt
v += val1 - val0;                  // => +chg points per 1000 ms
```

i.e. it treats `chg` (= `ATR_REGENERATEHP`/`ATR_REGENERATEMANA`) as **points gained per
second**. The original treats the same attribute as **seconds elapsed per +1 point** — the
exact inverse.

Concrete effect: a sleep routine (or a regenerating creature) that sets `REGENERATEHP = 5`
heals +1 HP every 5 s in the original (0.2 HP/s) but +5 HP/s in OpenGothic — 25x too fast,
and the discrepancy grows linearly with the attribute value. Values of `1` happen to match;
everything else diverges. OpenGothic's branch is also wrong in shape: it adds `chg` points
*per second continuously* instead of one point per discrete interval, and it permits negative
`chg` to drain HP/mana (the `// check health, in case of negative chg` path), whereas the
original ignores any non-positive regen attribute entirely.

## Proposed patch

Reinterpret `chg` as the seconds-per-point interval and emit at most the integer number of
ticks that elapsed over `[tick-dt, tick]`, gated on `chg > 0` like the original. This stays
stateless (no new members) by quantizing against `tick` on a `chg*1000` ms grid — functionally
equivalent to the original's per-NPC countdown timer in steady state.

OLD (`game/world/objects/npc.cpp:2337`):
```cpp
void Npc::tickRegen(int32_t& v, const int32_t max, const int32_t chg, const uint64_t dt) {
  uint64_t tick = owner.tickCount();
  if(tick<dt || chg==0)
    return;
  int32_t time0 = int32_t(tick%1000);
  int32_t time1 = time0+int32_t(dt);

  int32_t val0 = (time0*chg)/1000;
  int32_t val1 = (time1*chg)/1000;

  int32_t nextV = std::max(0,std::min(v+val1-val0,max));
  if(v!=nextV) {
    v = nextV;
    // check health, in case of negative chg
    checkHealth(true,false);
    }
  }
```

NEW:
```cpp
void Npc::tickRegen(int32_t& v, const int32_t max, const int32_t chg, const uint64_t dt) {
  // NOTE: in original-game oCNpc::Regenerate (Gothic2.exe @0x00741fd0) ATR_REGENERATEHP/
  // ATR_REGENERATEMANA are the number of *seconds between each +1 point* (the regen timer is
  // reloaded to regen*1000ms and a single point is added on each expiry), not points-per-
  // second. Non-positive regen does nothing. This is the path ZS_Sleep drives for sleep regen.
  uint64_t tick = owner.tickCount();
  if(tick<dt || chg<=0)
    return;

  const uint64_t period = uint64_t(chg)*1000; // ms between each +1 point
  int32_t ticks = int32_t(tick/period - (tick-dt)/period);
  if(ticks<=0)
    return;

  int32_t nextV = std::min(v+ticks,max);
  if(v!=nextV) {
    v = nextV;
    checkHealth(true,false);
    }
  }
```

Grep-verified symbols used: `Npc::tickRegen` (`npc.h:478`, `npc.cpp:2337`), `checkHealth`
(`npc.h:517`, `npc.cpp:558`), `owner.tickCount()` (used throughout `npc.cpp`),
`ATR_REGENERATEHP=6` / `ATR_REGENERATEMANA=7` (`game/game/constants.h:479-480`),
call sites `npc.cpp:2413`/`2415`.

Note the `max` clamp is preserved (original gates each +1 on `v < max`); the lower `std::max(0,...)`
clamp is dropped because non-positive `chg` now early-returns, matching the original which never
decrements via this path.
