# Heal/Regen parity: REGENERATE-over-time rate is inverted (rate vs. period)

**Confidence:** Medium-High. The code-level divergence is certain (the two formulas
compute reciprocal quantities). Vanilla-observability is the only uncertain part:
unmodded Gothic 2 NOTR leaves `ATR_REGENERATEHP` / `ATR_REGENERATEMANA` at 0 for most
NPCs, so the bug is dormant until any NPC/creature/mod sets these attributes non-zero,
at which point the regeneration rate is wrong (reciprocal) for every such NPC.

## Original function + address (prose only)
`oCNpc::Regenerate` at `0x00741fd0` (source tag `_ulf/oNpc.cpp`) drives passive
HP/MANA regeneration over time. For a living NPC it maintains two per-NPC float
**count-down accumulators** (one for HP, one for MANA). Each call it subtracts the
elapsed frame time (a global frame-delta in milliseconds) from each accumulator. When
the HP accumulator reaches zero-or-below, *and* `ATR_REGENERATEHP > 0`, *and* current
HP is below max HP, it adds exactly **+1** HP (via `oCNpc::ChangeAttribute`, index 0,
which itself clamps to max) and then **resets the accumulator to `ATR_REGENERATEHP * 1000`
milliseconds**. The MANA branch is identical using `ATR_REGENERATEMANA` (index 7) and
ChangeAttribute index 2.

The load-bearing consequence: the spacing between two consecutive +1 increments equals
`ATR_REGENERATEHP * 1000 ms = ATR_REGENERATEHP seconds`. So the attribute is the
**number of seconds per +1 point** — i.e. a *period*. A larger value means *slower*
regeneration; the effective rate is `1 / ATR_REGENERATEHP` points per second. The
helper `oCNpc::ChangeAttribute` (`0x0072ff60`) confirms the attribute layout (base
`0x1b8`, index 6 = REGENERATEHP, index 7 = REGENERATEMANA) and that each increment is a
single +1 that is clamped to the matching max attribute.

## OpenGothic file:line
`/Users/admin/Downloads/opengothic/game/world/objects/npc.cpp:2366-2382`
(`Npc::tickRegen`, called from `Npc::tick` at lines 2442 / 2444 with
`hnpc->attribute[ATR_REGENERATEHP]` / `[ATR_REGENERATEMANA]` as `chg`).

## Divergence
`Npc::tickRegen` interprets `chg` (= `ATR_REGENERATEHP`/`MANA`) as a **rate** —
points gained *per second* — instead of a **period** (seconds per +1 point):

```
int32_t val0 = (time0*chg)/1000;   // time0 = tick%1000
int32_t val1 = (time1*chg)/1000;   // time1 = time0+dt
int32_t nextV = std::max(0,std::min(v+val1-val0,max));
```

Over one wall-clock second this raises `v` by `chg`, i.e. **`chg` points/second**. The
original raises `v` by `1` point every `chg` seconds, i.e. **`1/chg` points/second**.
The two are reciprocals and agree only at `chg == 1`. Example: `ATR_REGENERATEHP = 5`
yields OpenGothic +5 HP/s but the original +0.2 HP/s — a 25x error; mana behaves
likewise. (A secondary, smaller faithfulness gap: the current code adds nothing for
`chg < 0`-handling and, with negative `chg`, would silently drain HP and run
`checkHealth`; the original ignores non-positive regen attributes entirely.)

## Proposed patch
Reinterpret `chg` as a period (seconds per +1 point), keeping the existing
stateless-on-global-tick phasing and the existing clamp. All referenced symbols
(`owner.tickCount()`, `checkHealth`, the `v`/`max`/`chg`/`dt` params) already exist in
this function.

OLD (`game/world/objects/npc.cpp:2366`):
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
  uint64_t tick = owner.tickCount();
  // NOTE: in original-game oCNpc::Regenerate @0x00741fd0 the regen attribute is a
  // PERIOD: it grants +1 point every (chg*1000) ms, i.e. one point per `chg` seconds.
  // (Attribute layout/clamp confirmed in oCNpc::ChangeAttribute @0x0072ff60.)
  // Non-positive regen attributes are ignored by the original; never decrement here.
  if(tick<dt || chg<=0)
    return;
  uint64_t period = uint64_t(chg)*1000; // ms between two +1 increments

  int32_t add = int32_t(tick/period) - int32_t((tick-dt)/period);
  if(add<=0)
    return;

  int32_t nextV = std::min(v+add,max);
  if(v!=nextV) {
    v = nextV;
    // check health, in case of script-driven negative max (clamp only)
    checkHealth(true,false);
    }
  }
```

Faithfulness note: the original adds at most +1 per `Regenerate` call and phases its
count-down off a per-NPC accumulator; this stateless reimplementation phases off the
global tick (as the existing code already does) and may grant more than one point in a
single very long frame. Both approximations are dominated by the corrected *rate*,
which is the behavioral divergence. If a byte-exact phase is later required, add two
per-NPC `int64_t` accumulator fields mirroring the original's `0x7c4` / `0x7c8`.
