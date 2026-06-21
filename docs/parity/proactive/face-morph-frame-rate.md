# Face/Morph-Ani frame advance rate ignores the script `speed` for timed anis

**Confidence:** High

## Original function + address

The morph-mesh playback in the original `Gothic2.exe` is driven by
`zCMorphMesh::AdvanceAnis` (`0x005a6830`), with per-ani parameters set up at load
time by `zCMorphMeshAni::Load` (`0x005a3290`, the `.mmb` binary path) and
`zCMorphMeshProto::ReadMorphAni` (`0x005a5160`, the ASCII `.mds` morph-script path);
running instances are created by `zCMorphMesh::StartAni` (`0x005a6f30`).

Key facts established from those functions (prose only):

- A `zCMorphMeshAni` stores, among others: `blend_in (+0x28)`, `blend_out (+0x2c)`,
  `duration (+0x30)`, `layer (+0x34)`, `speed (+0x38)`, `flags (+0x3c, 1 byte)`,
  `frame_count (+0x48)`. The binary field order in `Load` is exactly
  blend_in, blend_out, duration, layer, speed, flags — identical to ZenKit's
  `MorphMesh.cc` loader (`anim.duration` then `anim.speed`).
- In `ReadMorphAni` the ASCII columns are converted with fixed scales:
  `duration = col * 1000.0` (so `duration` ends up in **milliseconds**) and
  `speed = col * 0.001` (so `speed` ends up in **frames per millisecond**).
  `speed` and `duration` are two **independent** script values; `speed` is NOT
  derived from `frame_count / duration`.
- In `AdvanceAnis`, the playing frame position is advanced every tick by
  `frameTime += dt * speed`, then wrapped modulo `frame_count`. The advance rate is
  therefore **always** governed by `speed` (i.e. `1/speed` ms per frame), regardless
  of `duration` or any flag. `duration` is consumed by a separate hold/fade-out
  state machine (state `+0x28` of the running entry: blend-in -> hold-for-`duration`
  -> blend-out), and the loop flag (`flags & 2`, set for names containing `L` or for
  negative `duration`) only suppresses that auto fade-out — it does **not** change
  the frame-advance rate.

Net: the original plays every morph-ani's frame cycle at `1/speed` ms/frame and loops
that cycle for the whole `duration`.

## OpenGothic file:line

`game/graphics/mesh/protomesh.cpp:372` — `ProtoMesh::mkAnimation`, specifically the
`tickPerFrame` selection at lines 380-382:

```cpp
if(a.flags&0x2 || a.duration<=0)
  ret.tickPerFrame = size_t(1.f/a.speed); else
  ret.tickPerFrame = size_t(a.duration/float(a.frame_count));
```

`tickPerFrame` is then used as the ms-per-frame divisor in
`VisualObjects::preFrameUpdateMorph` (`game/graphics/visualobjects.cpp:529-545`,
`time/anim.tickPerFrame % anim.numFrames`).

## Divergence

OpenGothic only uses the correct `1/speed` rate when the ani is a loop ani
(`flags & 0x2`) or has non-positive duration. For the common case of a **non-looping
morph-ani with a positive duration** (most face expressions / mood anis authored with
a real `speed` AND a `duration`), OpenGothic instead derives the rate from
`duration / frame_count`, i.e. it stretches exactly one frame-cycle across the whole
duration. The original always advances at the script `speed` and loops the cycle for
the duration. The two agree only in the accidental case `speed == frame_count/duration`;
otherwise the face animation plays at the wrong speed (and does not loop its frame
cycle as the original does within the hold window).

## Proposed patch

Always honour the script `speed` for the frame-advance rate, matching
`zCMorphMesh::AdvanceAnis`. Keep a guard for `speed == 0` (frozen frame) so the
existing `tickPerFrame==0 -> 1` clamp still applies and there is no division by zero.

OLD (`game/graphics/mesh/protomesh.cpp:380-385`):
```cpp
  if(a.flags&0x2 || a.duration<=0)
    ret.tickPerFrame = size_t(1.f/a.speed); else
    ret.tickPerFrame = size_t(a.duration/float(a.frame_count));

  if(ret.tickPerFrame==0)
    ret.tickPerFrame = 1;
```

NEW:
```cpp
  // NOTE: in original-game zCMorphMesh::AdvanceAnis @0x005a6830 the frame position is
  // advanced every tick by frameTime += dt*speed and wrapped modulo frame_count, so the
  // frame-advance rate is ALWAYS 1/speed ms/frame (speed is stored frames-per-ms; see
  // zCMorphMeshAni::Load @0x005a3290 / ReadMorphAni @0x005a5160). 'duration' only feeds a
  // separate hold/fade-out state machine, it is not a frame-rate. Do not derive the rate
  // from duration/frame_count.
  if(a.speed>0)
    ret.tickPerFrame = size_t(1.f/a.speed); else
    ret.tickPerFrame = 0;

  if(ret.tickPerFrame==0)
    ret.tickPerFrame = 1;
```

Grep-verified symbols: `ProtoMesh::mkAnimation` and `Morph::tickPerFrame`/`duration`/
`numFrames` (`game/graphics/mesh/submesh/staticmesh.h:31-40`), `zenkit::MorphAnimation`
fields `speed`/`duration`/`flags`/`frame_count` (`lib/ZenKit/include/zenkit/MorphMesh.hh:16-38`,
loaded in field order in `lib/ZenKit/src/MorphMesh.cc:50-58`). `ret.duration` (line 378)
is left unchanged — it already maps the ms hold window used as `timeUntil` in
`VisualObjects::startMMAnim`.
