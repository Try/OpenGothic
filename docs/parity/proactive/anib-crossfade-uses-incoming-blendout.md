# Anim cross-fade uses the INCOMING animation's blendOut instead of the OUTGOING animation's

**Confidence:** Medium

## Original function + address (prose only)

In `Gothic2.exe` the active-animation cross-fade is a *two-weight* system, not a
single lerp:

- `zCModel::StartAni` (entry `0x0057b0c0`) creates a `zCModelAniActive` for the
  newly started animation and stores the animation's blend-in value (`zCModelAni`
  field at +0x70) and blend-out value (field +0x74) onto the active-ani record
  (active fields +0x28 and +0x2c respectively).
- `zCModelAni::GetBlendingSec` (entry `0x00586100`) shows the meaning of those two
  floats: the blend-*in* rate is `1.0 / (ani+0x70)` and the blend-*out* rate is
  `-1.0 / (ani+0x74)`. So +0x70 is the *enter* time and +0x74 is the *leave* time.
- `zCModel::FadeOutAni` (entry `0x0057f020`) is what runs when an animation is
  being replaced/stopped: per affected node it sets that node's fade rate to the
  *outgoing* active-ani's stored blend-out (`active+0x2c`, i.e. the outgoing
  ani's +0x74) and marks it "fading out" (state 2). For the surviving/incoming
  master ani it sets the node rate from that master's blend-*in* (`master+0x28`,
  i.e. +0x70).
- `zCModelNodeInst::CalcBlending` (`0x0057f720`) then slerps the per-node weighted
  contributions.

Net behaviour: when animation B replaces animation A, the visible cross-fade is
governed by **A.blendOut (outgoing leave time)** ramping down and **B.blendIn
(incoming enter time)** ramping up, i.e. its duration is effectively
`max(B.blendIn, A.blendOut)`. The incoming animation's *own* blendOut
(`B.blendOut`) is never consulted while B is entering — blendOut only describes
how an animation *leaves*.

## OpenGothic file:line

`game/graphics/mesh/pose.cpp:391` (inside `Pose::updateFrame`):

```cpp
const uint64_t blendMax = std::max(s.blendOut,s.blendIn);
const uint64_t blend    = std::max<uint64_t>(0, now-sBlend);
...
if(blend < blendMax) {
  float a2 = float(blend)/float(blendMax);
  base[idx] = mix(prev[idx],smp,a2);   // prev[] = frozen snapshot of the OLD pose
}
```

Here `s` is the **incoming** sequence. `prev[idx]` is the frozen snapshot of the
previous (outgoing) pose, captured in `onRemoveLayer` (`pose.cpp:549-557`) when the
layer was replaced. So this single lerp *is* the A→B cross-fade, and its duration
is taken as `max(s.blendOut, s.blendIn)` = `max(B.blendOut, B.blendIn)`.

Grep-verified symbols: `Animation::Sequence::blendIn` / `blendOut`
(`animation.h:107-108`, populated from MDS `blend_in`/`blend_out` in
`animation.cpp:219-220`); `Pose::Layer{seq,sAnim,sBlend,comb,bs}` (`pose.h:100`).
`blendOut` is referenced in exactly one place in the whole `game/` tree — this
line — so there is no separate fade-out path; the entire blend-out concept is
folded into this `max()`.

## Divergence

OpenGothic uses the **incoming** animation's `blendOut` (`B.blendOut`) as a proxy
for the transition's fade-out timescale. The original uses the **outgoing**
animation's blendOut (`A.blendOut`). They only coincide when consecutive
animations share blend times (common for symmetric Gothic anims, which masks the
bug), but they differ whenever the incoming ani has an asymmetric blendIn/blendOut
or the outgoing ani's blendOut differs from the incoming one's. Concretely, an
incoming ani with `blendIn=0.1, blendOut=0.5` will cross-fade in over 0.5s in
OpenGothic but over `max(0.1, A.blendOut)` in the original — visibly too slow
whenever the previous animation had a short blendOut.

## Proposed patch (DEFERRED — needs Layer plumbing, not a one-line constant)

The faithful fix is to capture the transition duration **at the moment the layer
is replaced**, where both the outgoing sequence (old `i.seq`) and the incoming
sequence are in scope, and store it on the `Layer`; `updateFrame` then uses the
stored value instead of recomputing from the incoming `s`.

NOTE: in original-game zCModel::FadeOutAni @0x0057f020 / zCModel::StartAni
@0x0057b0c0 the cross-fade fade-out rate comes from the OUTGOING ani's blendOut
(+0x74) while the incoming ani enters via its blendIn (+0x70); the incoming ani's
own blendOut is never used to enter.

Sketch (illustrative; multiple sites must set `blendDur`, so this is filed as a
proposal rather than applied):

- `pose.h` Layer: add `uint64_t blendDur = 0;`
- `pose.cpp` `startAnim` replace path (`~205-209`):
  OLD: `i.seq = tr ? tr : sq; i.sAnim = tickCount; i.sBlend = 0;`
  NEW: also `i.blendDur = std::max((tr?tr:sq)->blendIn, /*outgoing*/ i.seq->blendOut);`
  (read the outgoing `i.seq->blendOut` *before* reassigning `i.seq`).
- `pose.cpp` `addLayer` (fresh layer, no predecessor): `l.blendDur = sq->blendIn;`
- `pose.cpp` `processLayers` transition→next (`~299`):
  `l.blendDur = std::max(next->blendIn, /*outgoing transition*/ l.seq->blendOut);`
  (capture before `l.seq = next`).
- `pose.cpp` `updateFrame` (`391`): replace
  OLD: `const uint64_t blendMax = std::max(s.blendOut,s.blendIn);`
  NEW: `const uint64_t blendMax = blendDur;` (pass `i.blendDur` into `updateFrame`).
- `load()` (`~76-81`): default `i.blendDur = i.seq->blendIn;` (or serialize it).

Reason for DEFERRED: OpenGothic models the outgoing pose as a single frozen
snapshot rather than the original's continuously-weighted two-ani blend, so no
single duration reproduces the original exactly; and the minimal correct change
touches the `Layer` struct plus every layer-construction site (above) rather than
swapping one constant. The current `max(s.blendOut,s.blendIn)` is a defensible
heuristic for symmetric anims, so this is a real-but-low-blast-radius parity gap
that should be fixed as the small refactor above, not a blind one-liner.
