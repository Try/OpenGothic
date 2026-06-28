# NPC water behavior parity sweep — waynet water gating (oCWaypoint::CanBeUsed)

**Confidence:** NO FINDING (architectural / feature-gap, not a surgical 1:1 divergence)

## Scope examined
Engine-side NPC swim/water AI in OpenGothic vs `Gothic2.exe`:

- `MoveAlgo::tick` water interaction / swim-dive entry physics
  (`game/game/movealgo.cpp:235-470`) vs `zCAIPlayer::CalcStateVars` @0x0050e440.
  The geometry-driven wade/swim/dive classification (depth vs swim/dive thresholds)
  is conceptually faithful and already carries prior NOTE citations
  (water-deep-entry-swim-skip.md etc.). No fresh divergence.
- `oCAniCtrl_Human::GetWaterLevel` @0x006b89d0 / `::IsInWater` @0x006b8a40
  (water-level 1=swim, 2=dive gates) — already mirrored by the existing
  `isSwim()/isDive()` NOTE citations in `npc.cpp` (lines 563, 2196, 2271, 4190).
- `oCNpc::CanSwim` @0x00680930 / `oCNpc::CanDive` @0x00680900 — both read the
  per-NPC swim/dive-time fields (set by `oCNpc::SetSwimDiveTime` @0x00741f70,
  offsets 0x7cc/0x7d0). Their ONLY caller is `oCWaypoint::CanBeUsed` @0x0077df90,
  not the movement state machine.
- Dive-time drowning (`npc.cpp:2567`) — gated on `isDive()`, which in OpenGothic is
  only ever entered from player input (`MoveAlgo::startDive` is called solely from
  `playercontrol.cpp:898`); NPCs never auto-dive.

## Observed architectural gap (documented, not patched)
`oCWaypoint::CanBeUsed` @0x0077df90 makes an underwater waypoint usable by an NPC
only if that NPC `CanDive()` (for dive-waypoints) or `CanSwim()` — i.e. the original
waynet routes non-swimming NPCs around water-tagged waypoints.

OpenGothic loads the corresponding flag (`Waypoint::underWater`,
`game/world/waypoint.h:33`, from `dat.under_water`, `waypoint.cpp:22`) but never
consults it: the pathfinder applies no water/swim gate (`WayMatrix::adjustWaypoints`
even leaves a literal `//NOTE: what about water?` placeholder at
`game/world/waymatrix.cpp:198`). So OpenGothic's waynet ignores water entirely when
routing NPCs.

This is a missing-feature / architectural gap spanning the pathfinder, not a one- or
two-line behavioral divergence in existing logic, and its practical effect is narrow
(most guilds have nonzero swim/dive time, so `CanSwim/CanDive` return true anyway).
Per the clean-room rules ("architectural/script ⇒ NO FINDING; empty beats false
positives") it is reported here for the record rather than patched.

## Conclusion
No surgical, build-verifiable engine-side NPC water-AI divergence found. The
high-confidence wins in this area appear already harvested (dense existing NOTE
coverage in `movealgo.cpp` / `npc.cpp`). Remaining differences (waynet water
routing, NPC auto-dive/drown) are architectural feature-gaps. NO FINDING.
