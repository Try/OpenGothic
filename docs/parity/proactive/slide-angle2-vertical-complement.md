# Slide upper-angle bound (slide_angle2) uses wrong reference axis

**Confidence:** High (logic divergence is certain from the decompile; in-game magnitude
depends on the script's `SLIDE_ANGLE2` guild value, which is set at runtime by Daedalus
and could not be read statically from the binary).

## Original function + address (prose only)

The player slope-slide gate lives in `zCAIPlayer::CheckFloorSliding` @ `0x0050d4d0`.
It takes the floor-contact normal from the collision object, reads `normal.y`, clamps it
to [-1, 1], and computes the slope steepness as `acos(normal.y)` — i.e. the angle measured
**from horizontal**: 0 rad for a flat floor, ~pi/2 for a vertical wall. It then applies a
two-sided band, both bounds expressed in that same from-horizontal angle:

- A guard term (in the leading OR that returns "do not slide") tests
  `slopeAngle <= field@0x4c`; sliding is therefore only allowed when `slopeAngle > field@0x4c`.
- Further down, the slide is actually *started* only when `slopeAngle < field@0x54`.

So the original slides exactly when `field@0x4c < slopeAngle < field@0x54`.

`oCAniCtrl_Human::SetScriptValues` @ `0x006a5110` shows what those two fields are: it copies
the guild-value columns into the player and converts both to radians by multiplying by
`pi/180` (`0x3c8efa35`). The 9th and 10th `C_GILVALUES` columns — `SLIDE_ANGLE` and
`SLIDE_ANGLE2` — land in `field@0x4c` and `field@0x54` respectively (the 6th/7th/8th columns
`STEP_HEIGHT`/`JUMPLOW_HEIGHT`/`JUMPMID_HEIGHT` go to `0x2c`/`0x44`/`0x48`, which cross-checks
the offsets). **Both** `slide_angle` and `slide_angle2` are therefore plain slope angles
measured from horizontal, compared directly against `acos(normal.y)`. `slide_angle` is the
lower (must-be-this-steep) bound; `slide_angle2` is the upper (steeper-than-this is a wall,
do not slide) bound.

## OpenGothic file:line

`game/game/movealgo.cpp:600` (`MoveAlgo::testSlide`), `:626` (`slideAngle`), `:632` (`slideAngle2`).

OpenGothic stores the contact normal and tests `slideEnd < norm.y < slideBegin` where
`norm.y = cos(slopeAngle)`:

```
slideBegin = std::min(slideAngle(), 1.f);   // slideAngle()  = sin((90 - slide_angle )*k) = cos(slide_angle )
slideEnd   = slideAngle2();                  // slideAngle2() = sin(      slide_angle2 *k) = cos(90 - slide_angle2)
```

- `norm.y < slideBegin` ⟺ `slopeAngle > slide_angle`  — lower bound is correct and matches the original.
- `norm.y > slideEnd`   ⟺ `slopeAngle < 90 - slide_angle2`  — **upper bound is `90 - slide_angle2`,
  not `slide_angle2`.**

## Divergence

`slideAngle()` correctly maps the from-horizontal `slide_angle` into a `norm.y` threshold by
using the `sin(90 - x) = cos(x)` identity. `slideAngle2()` omits the `90 -` complement, so it
treats `slide_angle2` as if it were measured **from vertical**. As a result OpenGothic's upper
slide bound is `90 - slide_angle2` instead of the original's `slide_angle2`. The two functions
are asymmetric even though the original compares both guild values against the same
`acos(normal.y)` quantity.

Concretely, OpenGothic only slides when `slide_angle < slopeAngle < (90 - slide_angle2)`, which
requires `slide_angle + slide_angle2 < 90` for the band to be non-empty at all. With typical
values where `slide_angle2` is a steep upper limit (e.g. `slide_angle=35`, `slide_angle2≈50–70`),
the band is drastically narrowed or empty, so the player slides on a much smaller range of
slopes than the original Gothic2 (or fails to slide where the original would).

## Proposed patch

```
// game/game/movealgo.cpp  MoveAlgo::slideAngle2()
OLD:
  return std::sin(float(npc.world().script().guildVal().slide_angle2[gl])*k);
NEW:
  // NOTE: in original-game zCAIPlayer::CheckFloorSliding @0x0050d4d0 the slide band is
  // slide_angle < acos(normal.y) < slide_angle2, with BOTH bounds measured from horizontal
  // (SetScriptValues @0x006a5110 converts both guild columns deg->rad identically). Mirror
  // slideAngle() so slide_angle2 maps to a cos() threshold on norm.y instead of sin().
  return std::sin((90.f-float(npc.world().script().guildVal().slide_angle2[gl]))*k);
```

Grep-verified symbols: `MoveAlgo::slideAngle2`, `MoveAlgo::slideAngle`, `MoveAlgo::testSlide`
(`game/game/movealgo.cpp`); `IGuildValues::slide_angle2` / `slide_angle`
(`lib/ZenKit/include/zenkit/addon/daedalus.hh`, registered in `lib/ZenKit/src/addon/daedalus.cc`);
`GameScript::guildVal()` (`game/game/gamescript.h:107`). The change is one line, reuses the
existing `k` and `gl`, and is build-safe.
