# Climb-up ledge classification: JUMPUPLOW band ignores STEP_HEIGHT lower bound

**Confidence:** Medium-High (structural mapping is certain; gameplay magnitude depends on guild values, where STEP_HEIGHT and JUMPLOW_HEIGHT are often equal).

## Original function + address

`oCAniCtrl_Human::JumpForward` at `0x006b21ed` (and the parallel classifier
`oCAniCtrl_Human::CanJumpLedge` at `0x006b2050`) decide which climb-up animation
to start once a ledge has been detected (`zCAIPlayer::GetFoundLedge != 0`).

The original computes the ledge height delta `dY = ledge.Y - npc.Y`
(`*(GetLedgeInfo()+4) - *(this+0x7c)`) and then classifies it against THREE
distinct guild thresholds, stored on the AniCtrl at member offsets that
`oCAniCtrl_Human::SetScriptValues` (`0x006a5110`) fills, in struct order, from
the `C_GILVALUES` instance:

- offset `0x2c` <- `step_height`   (filled right after SWIM/DIVE time)
- offset `0x44` <- `jumplow_height`
- offset `0x48` <- `jumpmid_height`
- `jumpup_height` is consumed only by `SetJumpUpForceByHeight` (jump impulse),
  not used as a ledge-band edge here.

The original band logic (prose) is:

- `dY <= step_height`               -> plain forward JUMP (StartAni JUMP, returns 4)
- `step_height < dY <= jumplow_height` -> JUMPUPLOW   (StartAni 0xa08, returns 1)
- `jumplow_height < dY <= jumpmid_height` -> JUMPUPMID (StartAni 0xa14, returns 2)
- `dY > jumpmid_height`             -> JUMPUP      (StartAni 0x1070, returns 3)

i.e. the lowest climb band JUMPUPLOW has a **lower floor of `step_height`**:
anything a player can already step over is resolved as an ordinary forward jump,
never the hands-free climb-up-low pull.

The offset->field mapping was confirmed against ZenKit's `C_GILVALUES` field
order (`lib/ZenKit/include/zenkit/addon/daedalus.hh:25-32`:
water_depth_knee, water_depth_chest, jumpup_height, swim_time, dive_time,
**step_height, jumplow_height, jumpmid_height**), which mirrors the exe's
`SetScriptValues` assignment order.

## OpenGothic file:line

`game/world/objects/npc.cpp:4458` `Npc::tryJump()`, specifically the lowest-band
classification at lines 4539-4544:

```
4539  if(dY<=jumpLow) {
4541    ret.anim   = Anim::JumpUpLow;
4542    ret.height = jumpY;
4543    return ret;
4544  }
```

`jumpLow = float(g.jumplow_height[gl])` (line 4473). There is no `step_height`
lower bound: any positive `dY` up to `jumplow_height` (that survives the earlier
slide/path tests) is classified `JumpUpLow`.

## Divergence

OpenGothic's JUMPUPLOW band is `(0, jumplow_height]`; the original's is
`(step_height, jumplow_height]`. For a low ledge with `dY <= step_height`,
the original plays a plain forward **JUMP** while OpenGothic plays the
**JUMPUPLOW** climb-pull animation. The two inner edges (jumplow_height,
jumpmid_height) and the JUMPUP/JUMPUPMID bands already match the original.

Practical magnitude depends on the guild script: in stock Gothic II these
values are typically near-equal (STEP_HEIGHT ~= JUMPLOW_HEIGHT ~= 50), so the
diverging window `(0, step_height]` can be narrow or empty. It becomes visible
in mods (or guilds) where `step_height < jumplow_height`, producing a spurious
climb animation on knee-high obstacles that should be a forward hop.

## Proposed patch

Add the missing `step_height` floor to the JUMPUPLOW band, falling through to a
plain forward jump below it. All referenced symbols grep-verified:
`g.step_height` (zenkit `C_GILVALUES::step_height`, daedalus.hh:30),
`Anim::Jump` and `Anim::JumpUpLow` (npc.cpp uses both),
`ret.noClimb` (used at lines 4484/4499/4507/4522/4535).

OLD (`game/world/objects/npc.cpp` ~4473):
```
  const float jumpLow = float(g.jumplow_height[gl]);
  const float jumpMid = float(g.jumpmid_height[gl]);
  const float jumpUp  = float(g.jumpup_height[gl]);
```
NEW:
```
  const float stepH   = float(g.step_height[gl]);
  const float jumpLow = float(g.jumplow_height[gl]);
  const float jumpMid = float(g.jumpmid_height[gl]);
  const float jumpUp  = float(g.jumpup_height[gl]);
```

OLD (~4539):
```
  if(dY<=jumpLow) {
    // Without using the hands, just big footstep. Height: 50-100cm
    ret.anim   = Anim::JumpUpLow;
    ret.height = jumpY;
    return ret;
    }
```
NEW:
```
  // NOTE: in original-game oCAniCtrl_Human::JumpForward @0x006b21ed the JUMPUPLOW
  // band is (step_height, jumplow_height]; ledges at or below step_height resolve
  // to a plain forward jump, not a climb-up-low pull.
  if(dY<=jumpLow) {
    if(dY<=stepH) {
      ret.anim    = Anim::Jump;
      ret.noClimb = true;
      return ret;
      }
    // Without using the hands, just big footstep. Height: 50-100cm
    ret.anim   = Anim::JumpUpLow;
    ret.height = jumpY;
    return ret;
    }
```

DEFERRED note: if maintainers judge the `(0, step_height]` window to be already
covered by passive step-up (`MoveAlgo::stepHeight()` at movealgo.cpp:613, applied
during normal translate so such ledges rarely reach `tryJump`), the fix is still
correct for the explicit-jump-into-low-ledge case but its visible impact may be
small under stock guild values; treat as low-risk parity hardening rather than a
feel-changing fix.
