# DoT parity: drowning / dive-breath recovery is instant-reset instead of gradual

**Confidence:** High (behavioral divergence proven from the original disassembly).
The proposed *fix* is **DEFERRED** (not surgical — see below).

## Original function + address

The drown damage-over-time lives in an uncatalogued `oCNpc` per-frame handler at
**Gothic2.exe `0x0073e480`** (the block that reads the parser symbol
`NPC_DAM_DIVE_TIME` at `0x0073e57e`/`0x0073e58b`). Reconstructed behavior (prose
only):

- The NPC keeps a **dive-breath countdown** in `oCNpc+0x7d4`, initialized by
  `oCNpc::SetSwimDiveTime` (`0x00741f70`) to `dive_time * 1000` ms (the per-guild
  `GIL_VALUES` dive time), with the immutable maximum stored alongside in
  `oCNpc+0x7d0`.
- **While the NPC is in the dive movement-state** the handler decrements `0x7d4`
  by exactly one frame-time (`ztimer.frameTimeFloat`, ms) per frame. When `0x7d4`
  has fallen to `<= -NPC_DAM_DIVE_TIME` it deals **1 HP** via
  `oCNpc::ChangeAttribute(ATR_HITPOINTS, -1)` (`0x0072ff60`, which honours the
  godmode / IMMORTAL guards) and **resets `0x7d4` back to 0**. So drowning costs
  1 HP per `NPC_DAM_DIVE_TIME` ms spent past the dive limit; when HP reaches 0 it
  triggers the `T_DIVE_2_DROWNED` death animation.
- **While the NPC is NOT diving** (surfaced/swimming) the same handler *recovers*
  the breath countdown: `0x7d4 += 2 * frameTime` per frame (`fadd st,st`),
  clamped up to the maximum `0x7d0`. Breath therefore refills gradually, at twice
  real time, and a partially-spent breath meter **carries over** if the NPC dives
  again before it is full.

Net original behavior: tapping the surface for a moment does **not** restore a
full breath; you must stay up roughly `dive_time/2` seconds to fully recover, and
intermittent surfacing still lets you drown.

## OpenGothic file:line

- `game/world/objects/npc.cpp:2464-2479` — the dive DoT in `Npc::tick`.
- `game/game/movealgo.cpp:698-702` — `MoveAlgo::diveTime()` = `tickCount - diveStart`.
- `game/game/movealgo.cpp:838-840` — `diveStart` is reset on **every**
  Dive↔non-Dive transition (`game/game/movealgo.h:159` declares `diveStart`).

The per-tick damage logic itself matches the original: `npcDamDiveTime()`
(`game/game/gamescript.cpp:1641`) gives `NPC_DAM_DIVE_TIME`, and
`changeAttribute(ATR_HITPOINTS,-dmg,false)` yields 1 HP per tick past
`dive_time*1000` ms, with godmode/immortal handled in `Npc::changeAttribute`
(`game/world/objects/npc.cpp:1244`). That part is **correct** and is NOT the bug.

## Divergence

OpenGothic models the breath as a single timestamp (`diveStart`) and computes
`diveTime()` as `now - diveStart`. Because `diveStart` is hard-reset on every
dive transition (`movealgo.cpp:838`), **surfacing for even one frame fully
restores the entire dive-breath**, and `Npc::tick` then re-measures over-dive
time from zero. Consequences vs. the original:

1. There is **no gradual 2× recovery** — breath is binary (full while not diving).
2. There is **no carry-over depletion** — a player/NPC can avoid all drowning
   damage indefinitely by briefly breaking the surface and re-diving, which the
   original explicitly prevents. Drowning is effectively unreachable through
   intermittent surfacing in OpenGothic.

## Proposed patch — DEFERRED

A faithful fix requires replacing the timestamp model with a breath accumulator:
a new `MoveAlgo` field (e.g. `diveBreath`, separate from `diveStart` — which is
also used for dive-entry animation timing at `movealgo.cpp:341,768` and must not
be repurposed) that is decremented by `dt` while `isDive()`, incremented by
`2*dt` while surfaced (clamped to `dive_time*1000`), with the drown-damage
threshold and tick in `Npc::tick` driven off that accumulator instead of
`diveTime()`.

This is **not a surgical, build-trivial change**: it adds persistent NPC state
(serialization version bump + load/save in `MoveAlgo::load/save`,
`movealgo.cpp:32/41`), reworks the `Npc::tick` dive branch, and must preserve the
existing dive-entry animation uses of `diveStart`. Per the "surgical fix only,
else DEFERRED" rule it is deferred rather than risk a half-correct accumulator
or a serialization regression.

<!--
NOTE: in original-game the oCNpc per-frame dive handler @0x0073e480 (parser
symbol NPC_DAM_DIVE_TIME @0x0073e57e) depletes the dive-breath countdown
oCNpc+0x7d4 at 1x frametime while diving and refills it at 2x frametime while
surfaced (clamped to oCNpc+0x7d0), dealing 1 HP via oCNpc::ChangeAttribute
@0x0072ff60 each time the countdown passes -NPC_DAM_DIVE_TIME (then resetting it
to 0). OpenGothic's diveStart timestamp (game/game/movealgo.cpp:838) hard-resets
on every dive transition, so breath is fully restored by momentarily surfacing.
-->
