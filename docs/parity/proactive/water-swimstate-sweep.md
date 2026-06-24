# Water / swim-state subsystem parity sweep

**Confidence:** NO FINDING (no high-confidence, build-verifiable numeric divergence)

## Scope investigated
Walk -> swim -> dive thresholds, buoyancy/float height, swim->walk shallow-water
transition, dive/surface input, guild `water_depth_knee` / `water_depth_chest`,
weapon-disable-while-swimming, drown timing.

OpenGothic: `game/game/movealgo.cpp` (lines 230-341, 633-659, 754-766, 801-833),
`game/world/objects/npc.cpp` (2390-2404).

## Original functions consulted (prose only)
- `zCAIPlayer::CalcStateVars` @ 0x0050e440 — classifies water depth (`waterY - floorY`)
  against two body-relative thresholds (`+0x34` chest, `+0x38` knee, set in
  `zCAIPlayer::SetVob` @ 0x0050cad0 from model bbox height x engine constants at
  0x008314e4 / 0x008314e8) into level 0 (walk) / 1 (swim) / 2 (dive), stored at `+0x88`.
- `oCAniCtrl_Human::CheckWaterLevel` @ 0x006ab130, `GetWaterLevel` @ 0x006b89d0,
  `IsInWater` @ 0x006b8a40, `HackNPCToSwim` @ 0x006b5500.
- `oCNpc::SetSwimDiveTime` @ 0x00741f70 (swim/dive countdown = scriptValue x 1000),
  `CanSwim` @ 0x00680930, `CanDive` @ 0x00680900.
- `zCAIPlayer::DiveRotateX` @ 0x00511970 (dive pitch clamped to 80 deg = 0x42a00000).

## What was verified to be in parity (no change needed)
1. **Weapon-disable on swim entry** (movealgo.cpp:817-823). Original `CheckWaterLevel`
   @0x006ab130 sends put-away message `oCMsgWeapon(7)` when `GetWeaponMode()!=0` OR
   `HasTorch()`. Fist mode is weapon-mode 0, so fists are NOT put away. OG mirrors this
   exactly with `ws!=NoWeapon && ws!=Fist` for the weapon and an unconditional
   `dropTorch(true)`.
2. **Drown timing units** (npc.cpp:2390-2404). OG uses `guildVal().dive_time[gl]*1000`
   vs `diveTime()` in ms; original `SetSwimDiveTime` likewise multiplies the script
   dive-time by 1000 (0x447a0000). Units match.
3. **Guild water-depth units** (movealgo.cpp:633-641). `water_depth_knee/chest` consumed
   directly as world-unit (cm) offsets and compared in world Y, same as the original.
   No missing x100 scaling.
4. **Dive surface behavior** (movealgo.cpp:333-340). Buoyancy-driven surface
   (`pos.y+chest > water`) gated by a 2s minimum, not a forced timer; consistent with the
   engine clamping float height to `water-chest`.

## Why NO FINDING
The only structural differences observed fall outside the high-confidence,
build-verifiable bar mandated by the parity rules:
- Dive pitch: OG `setDirectionY(-40)` (movealgo.cpp:826) vs engine `DiveRotateX` clamp at
  80 deg — different abstraction (model pitch vs camera-relative dir), feel-tuning.
- Dive auto-transition debounce: OG `startDive()` requires `tickCount-diveStart>1000`
  (movealgo.cpp:758) and shares `diveStart` between the dive-debounce and the drown timer
  (`diveTime()`); the original's `0x88` level is recomputed every physics tick from
  geometry rather than from a 1s/2s timer. This is a clean-room reconstruction difference
  but the OG drown/breath path is explicitly DEFERRED, so no change is proposed.
- The depth->state thresholds in `CalcStateVars` derive from `zCAIPlayer` body-height
  constants (0x008314e4/e8), which are the engine fallback, NOT the Daedalus
  `water_depth_knee/chest` path that drives in-game NPCs. A 1:1 numeric comparison would
  require the raw float values of those data constants, which the warm decompiler could
  not resolve (`wde find` returned no data refs). Proposing a threshold flip without them
  would be a false positive.

DEFERRED rationale: empty beats false positives — no surgical numeric fix is justifiable
from the evidence gathered.
