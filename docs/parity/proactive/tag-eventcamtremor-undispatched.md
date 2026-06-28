# tag: `*eventCamTremor` (CAMERA_TREMOR) animation event is parsed but never dispatched

**Confidence:** High (divergence exists, well-isolated); **fix DEFERRED** (not surgically build-verifiable — see reason).

## Original fn + address
In the original `Gothic2.exe`, the per-frame model animation-event dispatcher
`zCModel::DoAniEvents` @0x0057b890 walks the active animation's event list and switches on
the event type. The camera-tremor type is handled by **case 0xb (11 = CAMERA_TREMOR)**: it
reads the model's root-node world position (model `+0x60`, node coords `+0x48/+0x58/+0x68`),
then calls `zCCamera::AddTremor(zCCamera::activeCam, pos, range, time, dir)`
(`zCCamera::AddTremor` @0x0054b660). The event-record fields are: `+0x6c` = `range`
(used as a **squared** camera-distance threshold), `+0x70` = `time` (tremor decay,
the camera stores `1/time` as its decay rate), and `+0x74/+0x78` = the tremor direction
vector (built as `(ev+0x74, ev+0x78, ev+0x74)`). `AddTremor` rejects the shake when the
squared camera→model distance is `>= range`, otherwise scales the direction vector by a
linear falloff `1 - dist²/range` and seeds the active camera's tremor state. Net effect:
every model that plays an `*eventCamTremor` event shakes the player's camera, attenuated by
distance (the classic heavy-monster stomp / dragon / golem screen shake).

## OG file:line
- `game/graphics/mesh/animation.cpp:32-56` — `Animation::Animation(...)` copies `ani.sfx`,
  `ani.sfx_ground`, `ani.events`, `ani.morph`, `ani.pfx`/`ani.pfx_stop` into `AnimData`,
  but **never copies `ani.tremors`**.
- `game/graphics/mesh/animation.cpp:429-467` — `Animation::Sequence::processEvents(...)`
  windows `events`, `gfx`, `mmStartAni` per tick; there is no tremor branch.
- `game/world/objects/npc.cpp:2521-2547` — `Npc::tickAnimationTags()` consumes `EvCount`
  (morph, groundSounds, def_opt_frame, fight mode, timed events); no camera-tremor path.

ZenKit *does* fully parse the event: `lib/ZenKit/src/ModelScriptDsl.cc:401-402, 476-483`
(`*eventCamTremor` → `MdsCameraTremor{frame, field1..field4}`) and stores it in
`ModelAnimation::tremors`. A repo-wide search shows `tremors` is referenced **only** inside
`lib/ZenKit` and never anywhere under `game/` — the data is loaded and then dropped.

## Divergence
`*eventCamTremor` is a dispatched animation event in the original (`DoAniEvents` case 0xb →
`zCCamera::AddTremor`), producing a distance-attenuated camera shake whenever a nearby model
plays the event. In OpenGothic the event is parsed by ZenKit into `ani.tremors` but is
discarded at load time (not copied into `AnimData`) and is dispatched nowhere, so animation
driven camera tremors never occur. This is a "tag parsed but not dispatched" gap, distinct
from the four already-handled/deferred items (DRAWSOUND C_SFX, DAM_MULTIPLY, SWAPMESH no-op,
DEF_WINDOW parade).

## Proposed patch
**DEFERRED.** A faithful, build-verifiable 1:1 fix is not surgical:

1. OpenGothic's camera has no `AddTremor` analogue. Its only screen-shake is the
   global-effects quake (`GlobalEffects::shake` → `Camera::shake`, `game/camera.cpp:744-748`),
   which is a per-tick random-walk offset with no squared-distance falloff and no
   `1/time` decay state — it cannot reproduce `AddTremor`'s
   `amp = (1 - dist²/range) * dir` + decay model without new camera state.
2. The `range/time/direction` semantics live in raw `zCModelAniEvent` offsets
   (`+0x6c/+0x70/+0x74/+0x78`). ZenKit exposes them only as unnamed
   `MdsCameraTremor::field1..field4`, so the field→(range², time, dir) mapping is not
   verifiable from OpenGothic/ZenKit source alone and would be guesswork.
3. A correct fix touches three layers (copy `ani.tremors` into `AnimData`; add a windowed
   tremor branch in `processEvents`/`EvCount`; add a distance-attenuated, decaying camera
   tremor to `Camera`), i.e. a feature, not a surgical parity patch.

Recommended follow-up when implemented, with citation:
`// NOTE: in original-game zCModel::DoAniEvents @0x0057b890 (case 0xb, CAMERA_TREMOR) calls`
`// zCCamera::AddTremor @0x0054b660 with the model root-node pos; AddTremor rejects when`
`// camera→model squared distance >= range(ev+0x6c), else shakes dir(ev+0x74,+0x78,+0x74)`
`// scaled by (1 - dist²/range) and decays at 1/time(ev+0x70).`
