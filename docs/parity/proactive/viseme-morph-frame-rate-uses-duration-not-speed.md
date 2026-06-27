# Viseme/morph-mesh frame rate computed from `duration/frame_count` instead of the authored `speed`

**Confidence:** High

## Original function + address (prose only)

In the original `Gothic2.exe`, the morph-mesh animation tick is driven by
`zCMorphMesh::AdvanceAnis` (address `0x005A6830`). For every active morph-ani it
advances a floating-point frame cursor by `frametime_ms * speed`, where `speed`
is the per-ani field at offset `0x38` of the `zCMorphMeshAni` proto, then wraps
that cursor modulo the frame count and linearly interpolates between the two
straddling frames. The frame rate is therefore governed *exclusively* by `speed`
(the ASC's authored frame rate, e.g. 25 fps -> `speed = 0.025` frames/ms ->
40 ms/frame).

The per-ani `duration` field (offset `0x30`, set in `zCMorphMeshProto::ReadMorphAni`
at `0x005A5160` as `durationSeconds * 1000`) is **never** used for frame timing.
It is consumed only as the default *hold time* inside `zCMorphMesh::StartAni`
(`0x005A6F30`): when a caller passes the `-2.0` sentinel as the hold-time
argument (as `oCNpc::StartStdFaceAni` `0x00738960` and the dialog face-ani path
in `oCNpc::OnMessage` `0x0074C080` do), `StartAni` substitutes the ani's own
`duration`. Its sign also feeds the loop flag (`0x2`). So in the original,
`duration` and `speed` are independent quantities and only `speed` sets the
playback rate.

## OpenGothic file:line

`game/graphics/mesh/protomesh.cpp:380-382` (in `ProtoMesh::mkAnimation`):

```cpp
if(a.flags&0x2 || a.duration<=0)
  ret.tickPerFrame = size_t(1.f/a.speed); else
  ret.tickPerFrame = size_t(a.duration/float(a.frame_count));
```

The resulting `tickPerFrame` is consumed by `VisualObjects::preFrameUpdateMorph`
(`game/graphics/visualobjects.cpp:529-546`) for `sample0`/`sample1`/`alpha`.

## Divergence

For a **non-looping, positive-duration** morph ani (`!(flags&0x2) && duration>0`),
OpenGothic computes ms-per-frame as `duration / frame_count`, whereas the original
always uses `1 / speed`. These match only when the script-authored `duration`
happens to equal `frame_count * (1/speed)`; when the artist's `duration` differs
from the ASC's natural length, OpenGothic plays the morph at the wrong rate.

Verified against the shipped MMB layout via `lib/ZenKit/tests/samples/morph0.mmb`
(zenkit field order blend_in, blend_out, duration, layer, speed, flags, ...):

| ani       | flags | duration | frames | speed | original `1/speed` | OG `duration/frames` |
|-----------|-------|----------|--------|-------|--------------------|----------------------|
| S_SHOOT   | 0     | 400      | 10     | 0.025 | 40 ms/frame        | 40 ms/frame (match)  |
| S_RELAX   | 0     | 1120     | 29     | 0.025 | 40 ms/frame        | 38.62 -> trunc **38** |

`S_RELAX` (and any face/emotion morph such as the `S_HOSTILE/S_ANGRY/S_NEUTRAL/
S_FRIENDLY` standard face anis, which run through the same `mkAnimation` path on
the head morph mesh) is played ~5% too fast and with an extra `size_t` truncation
error, because its authored `duration` (1120 ms) does not equal
`frame_count * 40 ms` (1160 ms). Looping visemes (`flags&0x2`) already take the
correct `1/speed` branch, so the visible regression is on non-looping face/morph
anis.

## Proposed patch

```cpp
// OLD (game/graphics/mesh/protomesh.cpp:380)
if(a.flags&0x2 || a.duration<=0)
  ret.tickPerFrame = size_t(1.f/a.speed); else
  ret.tickPerFrame = size_t(a.duration/float(a.frame_count));

// NEW
// NOTE: in original-game zCMorphMesh::AdvanceAnis @0x005A6830 the frame cursor is
// always advanced by frametime*speed (zCMorphMeshAni field 0x38); the per-ani
// `duration` (field 0x30) is used only as the default hold-time in
// zCMorphMesh::StartAni @0x005A6F30, never for frame timing. Deriving the rate
// from duration/frame_count plays anis whose authored duration differs from
// frame_count*(1/speed) at the wrong rate (e.g. sample S_RELAX: 40 vs 38 ms/frame).
ret.tickPerFrame = a.speed>0.f ? size_t(1.f/a.speed) : 0;
```

The existing guard immediately below (`if(ret.tickPerFrame==0) ret.tickPerFrame = 1;`,
`protomesh.cpp:384-385`) already covers the `speed<=0` / rounded-to-zero edge,
so no other change is needed.

NOTE citation for the patch site: original `zCMorphMesh::AdvanceAnis @0x005A6830`
advances by `frametime*speed`; `duration` (field 0x30) is hold-time only
(`zCMorphMesh::StartAni @0x005A6F30`).
