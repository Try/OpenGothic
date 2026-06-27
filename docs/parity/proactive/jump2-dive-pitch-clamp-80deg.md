# Dive look-pitch clamped to 90deg instead of original 80deg

**Confidence:** Medium

## Original function + address

`zCAIPlayer::DiveRotateX(float const&)` @ `0x00511970` is the routine that
adjusts the player's underwater dive pitch each frame while swimming/diving.
It measures the model's current forward-vector pitch (an `acos(...)` scaled by
the rad->deg factor `0x42652ee0` ~= 57.2958), adds the requested input delta,
and then clamps the resulting absolute pitch to the constant `0x42a00000`,
which is exactly **80.0 degrees**, before applying it via
`zCVob::RotateLocalX`. The sign branch keys off the forward vector's Y
component so the 80-degree cap applies symmetrically to nosing down and pitching
up. In other words, the original never lets the diving hero pitch more than
80 degrees away from horizontal; a perfectly vertical (straight-down / straight-up)
dive is not reachable.

## OpenGothic file:line

`game/world/objects/npc.cpp:457-467` — `Npc::setDirectionY(float rotation)`.

This is the single clamp point for the dive pitch. The accumulated pitch
(`rotY`, fed from mouse-Y and the `rspeed` turn rate) flows here from
`PlayerControl` (`game/game/playercontrol.cpp:710`, `:876`, `:884`), and the
clamped value is stored in `Npc::angleY` (degrees). `angleY` is consumed only
for diving: `MoveAlgo::applyRotation` uses `rotationYRad()` (= `angleY*pi/180`,
`npc.cpp:686`) to derive the dive vertical velocity component
(`out.y = -dpos.length()*sin(rot)`, `movealgo.cpp:528-533`), and
`mkPositionMatrix` only applies `angleY` when `mvAlgo.isDive()`
(`npc.cpp:5114`). The non-dive early-return at `npc.cpp:463` means the clamp
effectively governs dive pitch alone.

## Divergence

OpenGothic clamps the dive pitch to **+/-90 degrees**:

```
if(rotation>90)
  rotation = 90;
if(rotation<-90)
  rotation = -90;
```

The original caps the dive pitch magnitude at **80 degrees**. Net effect: in
OpenGothic the hero can angle a dive perfectly vertical (`sin(90deg)=1`, full
downward/upward swim velocity), whereas the original tops out at 80 degrees
(`sin(80deg)~=0.985`), so a truly straight-up/straight-down dive line is
slightly steeper than the original ever permitted. Observable as the dive
camera/swim trajectory being able to point dead-vertical in OpenGothic.

## Proposed patch

`game/world/objects/npc.cpp`, `Npc::setDirectionY`:

OLD:
```cpp
void Npc::setDirectionY(float rotation) {
  if(rotation>90)
    rotation = 90;
  if(rotation<-90)
    rotation = -90;
```

NEW:
```cpp
void Npc::setDirectionY(float rotation) {
  // NOTE: in original-game zCAIPlayer::DiveRotateX @0x00511970 the dive look-pitch
  // is clamped to a magnitude of 80deg (const 0x42a00000), not 90deg, so a perfectly
  // vertical dive line is unreachable.
  if(rotation>80)
    rotation = 80;
  if(rotation<-80)
    rotation = -80;
```

Grep-verified symbols: `Npc::setDirectionY` (npc.cpp:457), `Npc::angleY`
(npc.h:647), `Npc::rotationYRad` (npc.cpp:686), dive consumer
`MoveAlgo::applyRotation` (movealgo.cpp:528). The fmod/gating logic below the
clamp is unchanged.

Note on confidence: the 80.0 constant and the rad/deg conversion in
`DiveRotateX` are unambiguous; the residual uncertainty is only in the exact
axis the original `acos` measures from. OpenGothic's `angleY` is degrees from
horizontal (0=level, default dive entry is `setDirectionY(-40)`,
movealgo.cpp:839), which maps cleanly onto the original's pitch magnitude, hence
the +/-80 substitution. Marked Medium rather than High solely for that
measurement-reference caveat.
