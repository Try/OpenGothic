# zCEarthquake trigger does not shake the camera (OnTrigger is a stub)

**Confidence:** High (definitive: OG implements `Earthquake::onTrigger` as a no-op log; the original performs a camera tremor). The *fix* is DEFERRED (non-surgical).

## Original function + address (prose only)
- `zCEarthquake::OnTrigger` @ `0x00613f90` (and the identical `zCEarthquake::OnUntrigger` @ `0x00613fe0`):
  reads the earthquake vob's own world position (the vob translation columns at object
  offsets +0x48/+0x58/+0x68) and calls `zCCamera::AddTremor(activeCam, &vobPos, radiusSq,
  durationMs, &amplitudeVec)`. The radius value passed is the *squared* radius, the
  duration is in milliseconds, and the amplitude is a 3-component vector in cm.
- `zCEarthquake::zCEarthquake` @ `0x00613ea0` sets the defaults: amplitude vec = (2.0, 30.0, 2.0) cm
  (offsets +0x128/+0x12C/+0x130), squared-radius = 40000.0 (i.e. radius 200 cm, offset +0x120),
  duration = 5000.0 ms (offset +0x124).
- `zCEarthquake::Unarchive` @ `0x006140b0` shows the editor units: the archived `radius` is read
  then stored **squared** (`r*r`), and the archived `timeSec` is read then multiplied by 1000.0
  to become milliseconds. So the on-disk fields are linear radius (cm) and seconds; the runtime
  uses radius² and milliseconds.
- `zCCamera::AddTremor` @ `0x0054b660` is the actual shake math:
  1. compute squared distance `d2` between the active camera/listener position and the source `vobPos`;
  2. if `radiusSq <= d2` → return (no shake outside the radius);
  3. falloff factor `f = 1.0 - d2/radiusSq` (note: ratio of *squared* distances, so the attenuation
     is non-linear in actual distance, strongest at the epicenter, zero at the rim);
  4. scaled amplitude `= f * amplitudeVec`;
  5. seed the per-frame tremor state: tremor scale set to 1.0, decay rate `= 1.0/durationMs`; when a
     tremor is already running it takes the component-wise `max` of the amplitudes and keeps the
     slower (larger) of the two decay rates, so overlapping quakes combine rather than replace.

## OpenGothic file:line
- `game/world/triggers/earthquake.cpp:11-13` — `Earthquake::onTrigger` body is just
  `Log::d("TODO: earthquake, ...")`; no camera shake is produced.
- Data is available: `lib/ZenKit/include/zenkit/vobs/Misc.hh:411-424` exposes
  `VEarthquake::radius`, `VEarthquake::duration` (seconds), `VEarthquake::amplitude` (Vec3, cm).
- Existing (separate) shake plumbing that a real implementation could build on:
  `game/game/globaleffects.cpp:76-98` (`GlobalEffects::shake`) and `game/camera.cpp:744-748`
  (camera consumes `globalFx()->shake(sh)` into `Camera::shake`). The Quake effect type lives at
  `game/game/globaleffects.h:61-63`.

## Divergence
In the original, walking near a triggered `zCEarthquake` vob shakes the camera with an amplitude
that attenuates by `1 - dist²/radius²` and decays linearly over the configured duration. In
OpenGothic the trigger does nothing but write a debug log, so these vobs are silent — no camera
tremor, no radius falloff, no duration decay. This affects scripted set-pieces that rely on the
placed earthquake trigger (as opposed to the script command `Wld_PlayEffect("EARTHQUAKE.eqk")`,
which OG routes through the unrelated, non-positional `GlobalEffects::shake`).

## Proposed patch — DEFERRED

**Reason (non-surgical):** OpenGothic's only camera-shake mechanism (`GlobalEffects::shake` /
`Camera::shake`) is a *global, non-positional* screen shake: it ignores the source position and the
camera-to-source distance entirely (`game/globaleffects.cpp:76-98` overwrites `origin` with a
random-jitter offset and has no radius/falloff term). Faithfully reproducing `zCEarthquake` requires
a new *positional* tremor path that:
(a) reads the trigger vob's world position,
(b) computes the `1 - dist²/radius²` falloff against the live camera position each frame,
(c) drives the original tremor state machine (scale=1.0 seed, `1/durationMs` linear decay,
    component-wise `max`-combine of overlapping quakes).
None of that exists today, and the global `shake()` model can't express it without new state.
That is a feature subsystem, not a one-liner, so it exceeds the surgical/high-confidence bar.

A future implementer has everything needed: the ZenKit fields are present
(`radius`, `duration`, `amplitude`), `AbstractTrigger` carries the vob, and the camera already
consumes a `shake` offset. Any implementation must carry the citation, e.g.:
`// NOTE: in original-game zCEarthquake::OnTrigger @0x00613f90 -> zCCamera::AddTremor @0x0054b660: falloff = 1 - dist^2/radius^2, duration in ms, amplitude in cm, radius default 200cm, amplitude default (2,30,2), duration default 5s`.
