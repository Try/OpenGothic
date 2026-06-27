# Water: deep-water entry skips the swim transition (dead `3*chest` branch swallows the `else if` chain)

**Confidence:** Medium

## Original function + address (prose only)
In the original `Gothic2.exe`, an actor's water level is recomputed geometrically
**every physics tick**. `zCAIPlayer::CalcStateVars` (`0x0050e440`) takes the floor/water
spatial state and writes a water-level enum (0 = none/wade, 1 = swim, 2 = dive) directly
from the relation between the current water depth and two body-proportion thresholds
(the "knee" and "chest" thresholds at object offsets `+0x34`/`+0x38`, derived from the
model bounding box). `oCAniCtrl_Human::CheckWaterLevel` (`0x006ab130`) then drives the
swim/dive animation transition from that level, and `oCAniCtrl_Human::GetWaterLevel`
(`0x006b89d0`) / `IsInWater` (`0x006b8a40`) report it. Because the level is assigned
directly from the current geometry, **entering water always yields a swim/dive state
regardless of how fast the actor crosses the surface** — there is no per-frame
"approach window" that can be jumped over.

## OpenGothic file:line
`/Users/admin/Downloads/opengothic/game/game/movealgo.cpp:300-314`

## Divergence
OpenGothic's water transition is a single `if / else if` chain keyed on the depth above
the actor's feet (`gpos = max(pos.y, ground)`, `water` = surface):

```
if(gpos + 3.f*chest <= water) {        // depth >= 3*chest  -> DEAD no-op (dive disabled)
  // setState(Dive);                   // commented out
  }
else if(gpos + chest <= water+0.01f && hasSwimAnimations()) {  // depth >= chest -> Swim
  ...
  }
```

The first branch (`depth >= 3*chest`, i.e. very deep water) is an intentionally-disabled
placeholder for auto-dive — its body is entirely commented out. But because it is the
**first** arm of an `else if` chain, taking it **prevents the swim arm at line 304 from
being evaluated**. As long as the actor reaches deep water by gradually wading in, the
swim arm fires first (at `depth >= chest`) and `state==Swim` is latched, so the later deep
frames are harmless. The gap is a *one-tick* crossing straight into water deeper than
`3*chest` without ever spending a frame in the `chest .. 3*chest` window — e.g. falling
or being translated from above the surface into deep water in a single step. In that case
the swim transition (and `emitWaterSplash`) at line 304 is skipped, the actor falls
through to the "no longer in air" code at `movealgo.cpp:416`, takes
`npc.takeFallDamage(fallSpeed)` and is set to `Run` **underwater** instead of splashing
into `Swim`. The original never exhibits this because the water level is assigned
directly from geometry each tick rather than gated through a per-frame transition window.

## Proposed patch
Remove the dead deep-water branch so any `depth >= chest` uniformly enters the swim
handler. The existing inner guard `if(state!=Swim && state!=Dive)` already preserves an
in-progress dive or swim, so a diver in deep water is **not** forced back to Swim — the
merge is behaviorally identical for every case except the previously-unhandled deep entry.

OLD (`game/game/movealgo.cpp:300-304`):
```cpp
    const float gpos = std::max(npc.position().y, ground);
    if(gpos + 3.f*chest <= water) {
      // underwater walk bug-like case: can switch to dive here
      // setState(Dive);
      }
    else if(gpos + chest <= water+0.01f && npc.hasSwimAnimations()) {
```

NEW:
```cpp
    const float gpos = std::max(npc.position().y, ground);
    // NOTE: in original-game zCAIPlayer::CalcStateVars @0x0050e440 the water level
    // (0=wade,1=swim,2=dive) is assigned directly from geometry every tick, so entering
    // water always yields a swim/dive state regardless of approach speed. The disabled
    // `depth >= 3*chest` placeholder below must not swallow the swim transition on a
    // single-tick plunge into deep water (else: fall-damage + Run underwater).
    if(gpos + chest <= water+0.01f && npc.hasSwimAnimations()) {
```

Verified OG symbols used by the patch all exist in `movealgo.cpp`: `gpos`, `chest`
(`waterDepthChest()`), `water` (`waterRay`), `npc.hasSwimAnimations()`, `setState(Swim)`,
`emitWaterSplash`, `state`/`Swim`/`Dive`. The deleted branch contains no executable
statements, so removing it changes behavior only for the deep-entry case.
