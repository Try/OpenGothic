# Perception: horizontal sight FOV half-angle (91° vs 80°)

**Confidence: High — APPLIED**

## Original
`oCNpc::CanSee` (Gothic2.exe `0x00741c10`) computes the azimuth and elevation to the
target via `GetAngles` (degrees) and returns visible iff **`|azimuth| < 91` AND
`|elevation| < 91`** (both compared against the constant `0x5b` = 91). `GetAngle`
(`0x006811c0`) is `acos(dot(forward, dir)) * 57.2957`, i.e. degrees off the facing
axis. So the original forward vision cone is **±91°** horizontally (and ±91°
vertically — a near-hemisphere).

## OpenGothic (before)
`Npc::canRayHitPoint` and `Npc::canSeeItem` (`game/world/objects/npc.cpp`) used
`ref = cos(100°)` and tested `cos(da) <= ref`, where `da` is built from the
**reversed** (target→self) direction (`dx = self.x-pos.x`). Working the geometry
through, the effective forward half-angle is `180° − refAngle`, so `cos(100°)` gives
only a **±80°** cone — NPCs were ~11°/side too short-sighted laterally. (OG also omits
the original's vertical ±91° gate, but that is near-hemispherical and rarely matters.)

## Fix (applied)
Changed `ref` to `cos(89°)` in both `canRayHitPoint` and `canSeeItem`, so
`180° − 89° = 91°` reproduces the original's ±91° forward cone. The `angOverride`
callers are unaffected (they pass their own angle in the same convention). The vertical
gate is intentionally left out (very permissive; would only reduce vision for targets
nearly directly above/below).

This corrects the earlier Medium-confidence note: the geometry is now verified from
`CanSee`'s `0x5b` constants and the reversed-direction derivation. Build-verified;
widens NPC lateral vision to match vanilla (stealth-relevant), so worth an in-game pass.
