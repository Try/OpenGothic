# Swim/Dive parity: 2000 ms minimum-dive-duration gate before dive->swim surfacing

**Confidence:** Low-Medium (DEFERRED — concrete divergent behavior identified, but no citable
original constant; original logic is geometry-driven, so a "correct value" cannot be supplied,
and removing the gate risks animation flicker).

## Original fn + address (prose)
In `Gothic2.exe` the wade/swim/dive state is recomputed from geometry **every physics tick** and
carries **no time hysteresis**:

- `zCAIPlayer::CalcStateVars` @ `0x0050e440` derives the water level into the player field at
  `+0x88` purely from the body-vs-water depth `collObj[0xc4] - collObj[0xc0]` against the two
  per-guild thresholds at `+0x34` (knee, wade/swim boundary) and `+0x38` (chest, swim/dive
  boundary): `>= knee` -> 0 (wade), `chest..knee` -> 1 (swim), `< chest` -> 2 (dive). These
  thresholds are loaded by `zCAIPlayer::SetScriptValues` @ `0x006a5110` from the guild
  `water_depth_knee`/`water_depth_chest` columns.
- `oCAniCtrl_Human::CheckWaterLevel` @ `0x006ab130` then plays the wade/swim/dive transition
  animations off that recomputed level. The dive->swim surfacing therefore fires the instant the
  chest node rises back to the swim band; there is no minimum-dive-duration timer anywhere in this
  path. (The only dive timer the original keeps is the drown clock from
  `oCNpc::SetSwimDiveTime` @ `0x00741f70`, which is unrelated to the surface transition.)

## OG file:line
`/Users/admin/Downloads/opengothic/game/game/movealgo.cpp:341-348`

```cpp
if(dive) {
  if(pos.y+chest > water && std::isfinite(water)) {
    npc.tryTranslate(Tempest::Vec3(pos.x, water-chest, pos.z));
    if(npc.world().tickCount()-diveStart>2000) {   // <-- 2000 ms gate
      setState(Swim);
      }
    }
  return true;
  }
```

`diveStart` is (re)stamped to `tickCount()` on every Dive enter/exit
(`movealgo.cpp:841-842`), so `tickCount()-diveStart` is the elapsed dive time.

## Divergence
Once a diving NPC/player rises far enough that the chest clears the surface, OpenGothic clamps the
body to `water-chest` but keeps it in the **Dive** state (dive pose) until a hardcoded **2000 ms**
have elapsed since the dive began. In the original, surfacing flips dive->swim immediately on the
geometric water-level boundary with no such delay. Net effect: tapping dive and immediately
steering up leaves OpenGothic locked in the dive animation, floating at the surface, for up to ~2 s
before the swim animation engages.

## Proposed patch
DEFERRED.

Reason: the divergence is real and build-verifiable, but (1) the original behavior is a pure
geometry recompute with **no constant to cite** — the honest "fix" is to delete the timer, not to
correct a number; and (2) the `diveStart>2000` guard appears to be a deliberate anti-flicker /
minimum-dive heuristic guarding against non-planar water patches and per-tick water-level
oscillation near the surface (the same waterfall caveat called out at `movealgo.cpp:294`). Removing
it could reintroduce dive/swim animation chatter at the waterline. A high-confidence surgical fix
would require reproducing the original's per-tick water-level state machine
(`CalcStateVars`/`CheckWaterLevel`) rather than tweaking this one literal, which is out of scope for
a single-constant parity patch. Recommend leaving as-is until the swim/dive state machine is ported
wholesale.

Related (already tracked, not re-fixed here): dive-death water-level 2, deep-water swim-skip,
dive-pitch 80deg clamp (`DiveRotateX` @ `0x00511970`, `real_42a00000`=80.0 vs OG `setDirectionY`
clamp of 90deg), footstep-in-water, slide_angle2 complement.
