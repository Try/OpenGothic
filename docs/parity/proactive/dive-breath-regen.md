# Dive: breath does not regenerate gradually after surfacing

> DEFER: needs a persistent breath-deficit field (new state) + 2x-frametime decay while surfaced, replacing the diveStart-reset model; non-surgical and changes diving safety. Needs runtime tuning.

**Confidence:** Medium

## Original (Gothic2.exe)

The per-frame NPC breath/drown handler lives in an (un-named in Ghidra) `oCNpc`
member that begins at `0x0073e480` (the gap between `oCNpc::UpdateNextVoice`
`@0073e3c0` and `oCNpc::oCNpcTimedOverlay::GetMdsName` `@0073e8d0`). It reads the
Daedalus symbol `NPC_DAM_DIVE_TIME` (string `@0x8b8a28`, fetched at
`0073e57e..0073e5d5`) and plays `S_DROWNED` (`@0x8b1688`) on death.

Two NPC float fields drive it:
- `+0x7d0` = configured dive time = `dive_time * 1000` (ms), written by
  `oCNpc::SetSwimDiveTime @0x00741f70` (multiplier `447a0000` = 1000.0).
- `+0x7d4` = the *live* breath countdown (ms), initialised to `+0x7d0`.

A sentinel guard at `0073e535` skips all breath logic when `+0x7d0 == -1000000`
(breath disabled). Otherwise it branches on the ani-controller action mode
(`[+0x97c]+0x14c`):

- **Action mode 6 (DIVE)** — drain (`0073e554..0073e6d3`): each frame
  `breath -= frameTime`; once `breath <= -NPC_DAM_DIVE_TIME` it loses 1 HP
  (`ChangeAttribute(HITPOINTS, -1)`) **and resets `breath` to 0**, giving one HP
  loss per `NPC_DAM_DIVE_TIME` ms.
- **Any other mode (surfaced / swimming)** — regenerate (`0073e6d5..0073e713`):
  `breath += 2 * frameTime`, **clamped up to `+0x7d0`**.

So in the original the breath bar is a *persistent* value: surfacing refills it
only gradually (at twice the drain rate). Bobbing at the surface for one frame
does **not** restore a full breath bar.

## OpenGothic

`game/game/movealgo.cpp:688` — `diveTime()` returns `tickCount() - diveStart`.
`game/game/movealgo.cpp:828-830` — `diveStart` is reset to *now* on every
Dive↔non-Dive transition:

```
if((f==Dive) != (flags==Dive))
  diveStart = npc.world().tickCount();
```

`game/world/objects/npc.cpp:2382-2383` then uses `t = diveTime()` against
`v = dive_time[gl]*1000`.

Because `diveStart` is a single timestamp re-stamped the instant the NPC leaves
Dive, breath is **fully and instantly restored** the moment the head clears the
surface, regardless of how briefly. There is no gradual-regen / clamp path.

## Divergence

Gameplay-different: in the original a player who surfaces only briefly keeps a
partially-depleted breath bar and must spend ~half the dive time at the surface
to refill it; in OpenGothic a single surfacing frame fully refills breath,
making sustained underwater traversal (repeated short dives) strictly easier and
preventing the original's "you can't out-bob the timer" behaviour.

## Proposed patch

Track elapsed breath as an accumulator that regenerates at 2x while not diving,
instead of recomputing it from a re-stamped timestamp. Replace the timestamp
reset with deficit-carrying logic.

`game/game/movealgo.h` (near `diveStart`):

OLD:
```cpp
    uint64_t            diveStart  = 0;
```
NEW:
```cpp
    // NOTE: in original-game breath is a persistent value (oCNpc +0x7d4):
    // it drains while diving and regenerates at 2x frame-time while surfaced,
    // clamped to dive_time*1000 ms. It is NOT reset to full on surfacing.
    uint64_t            diveStart   = 0;  // last Dive-enter tick
    uint64_t            diveElapsed = 0;  // carried breath deficit (ms)
```

`game/game/movealgo.cpp:828-830`:

OLD:
```cpp
  if((f==Dive) != (flags==Dive)) {
    diveStart = npc.world().tickCount();
    }
```
NEW:
```cpp
  if((f==Dive) && !(flags==Dive)) {
    // NOTE: in original-game re-diving resumes from the carried deficit
    diveStart = npc.world().tickCount() - diveElapsed;
    }
  else if((f!=Dive) && (flags==Dive)) {
    // NOTE: in original-game surfacing only regenerates breath gradually (2x)
    diveElapsed = npc.world().tickCount() - diveStart;
    }
```

`game/game/movealgo.cpp` — in `tick(dt)` while not diving, regenerate the
deficit at twice frame time and clamp to zero:

OLD (add new code; no existing line to replace — insert in the non-dive branch
of `tick`):
```cpp
  // (no breath regeneration exists today)
```
NEW:
```cpp
  if(!isDive() && diveElapsed>0) {
    // NOTE: in original-game breath regenerates at 2x frame-time (oCNpc @0073e6e8)
    uint64_t regen = 2*dt;
    diveElapsed = (regen>=diveElapsed) ? 0 : diveElapsed-regen;
    }
```

(Persist `diveElapsed` alongside `diveStart` in `load`/`save`.)
