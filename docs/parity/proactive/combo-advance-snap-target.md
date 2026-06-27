# Combo-advance on timed press snaps to the wrong reference frame (DEF_HIT_END vs DEF_WINDOW end)

**Confidence:** Medium (concrete event-tag-array divergence confirmed; user-visible magnitude
and the exact OG-side re-implementation need runtime verification — see DEFERRED below)

## Original function + address

`oCAniCtrl_Human::HitCombo` @ `0x006b0260` is the per-tick animation-event handler that drives the
melee combo state machine. The per-combo-step table is parsed by `oCAniCtrl_Human::GetFightLimbs`
@ `0x006af1e0`, which fills a stride-`0x18` array of step records from the model's ani event tags:

- `DEF_OPT_FRAME`  -> record field `+0x1cc` (optimal/hit frame), and the step *count* into `+0x1b8`.
- `DEF_HIT_END`    -> record field `+0x1d0` (hit-end frame; this is the auto-advance trigger).
- `DEF_WINDOW`     -> record fields `+0x1d4` (input-window start) and `+0x1d8` (input-window end).
- `DEF_PAR_FRAME`  -> parry window.

When a buffered next-attack press (`this[0x1b0]` bit 0) is evaluated and the current play frame
falls inside the input window `[+0x1d4 .. +0x1d8]` (inclusive on both ends), `HitCombo` commits the
chain by fast-forwarding the *current* swing:

> it calls `SetActFrame(activeAni, window_end - 1)` and stores `window_end - 1` into its
> last-frame tracker `+0x1bc`, where `window_end` is the `DEF_WINDOW` end frame (record `+0x1d8`).

i.e. on a timed press the original snaps the playhead to the **end of the combo input window**
(one frame short), truncating the remainder of the current swing so the next swing begins.

## OpenGothic file:line

`game/graphics/mesh/pose.cpp:788-789` (inside `Pose::continueCombo`):

```
if(combo.len()<d.defHitEnd.size())
  prev->sAnim = tickCount - d.defHitEnd[combo.len()];
combo.incLen();
return prev->seq;
```

Array provenance confirmed in `game/graphics/mesh/animation.cpp:641-648`:
- `defHitEnd` <- `MdsEventType::HIT_END`   (original `DEF_HIT_END`, record `+0x1d0`)
- `defWindow` <- `MdsEventType::COMBO_WINDOW` (original `DEF_WINDOW`, record `+0x1d4`/`+0x1d8`)

## Divergence

On a valid in-window combo press, OpenGothic fast-forwards the swing to `defHitEnd[combo.len()]`
(the `DEF_HIT_END` frame), whereas the original snaps to the `DEF_WINDOW` *end* frame
(`defWindow[combo.len()*2 + 1] - 1`). These are two distinct event-tag arrays with distinct values
in the ani definition; in the original layout the input window end (`+0x1d8`) is positioned at/after
the hit-end (`+0x1d0`), so the original truncates the current swing more aggressively (snaps further
forward) than OpenGothic does. The result is that the chained attack's onset timing — how quickly the
next swing starts after a correctly-timed press — diverges from `Gothic2.exe`, affecting combo rhythm
and the feel/length of the recovery that gets cancelled.

This is distinct from the already-fixed lockout bug (continueCombo ignoring a mistimed press,
pose.cpp:771-777): that governs *whether* the chain advances; this governs *to what frame* the swing
jumps once it does.

## Proposed patch

**DEFERRED.** The divergence (snap reference = `DEF_HIT_END` instead of `DEF_WINDOW` end) is concrete
and grep-verified, but a build-safe surgical fix cannot be asserted high-confidence here because the
snap target is entangled with OpenGothic's single-animation combo model:

- `Animation::Sequence::isFinished` (`animation.cpp:241-245`) returns "finished" when
  `t > defHitEnd[comboLen]`, and `atkTotalTime` (`animation.cpp:252-258`) also keys off `defHitEnd`.
  Rewinding `sAnim` so that `t == defWindow[id+1]` (the window end, typically *greater than*
  `defHitEnd[len+1]`) risks immediately tripping `isFinished` on the next update and aborting the
  combo animation prematurely — the opposite of the intended responsiveness.

Candidate change (to be validated at runtime against the original before committing):

```
// OLD
if(combo.len()<d.defHitEnd.size())
  prev->sAnim = tickCount - d.defHitEnd[combo.len()];
combo.incLen();
return prev->seq;

// NEW
// NOTE: in original-game oCAniCtrl_Human::HitCombo @0x006b0260 a valid in-window combo press
// snaps the swing playhead to the DEF_WINDOW end frame (record +0x1d8, OG defWindow[id+1]),
// one frame short, not to DEF_HIT_END (record +0x1d0, OG defHitEnd). The window end sits
// at/after the hit-end, so the original truncates the current swing further forward.
if(id+1<d.defWindow.size())
  prev->sAnim = tickCount - d.defWindow[id+1];
combo.incLen();
return prev->seq;
```

Reason for DEFERRED: the correct snap value interacts with `isFinished`/`atkTotalTime`
(both defHitEnd-based) and with how `Pose` re-evaluates the next step's window; needs in-engine
verification that snapping to the window end does not immediately finish the combo sequence.
Symbols `combo`, `combo.len()`, `prev->sAnim`, `id`, `d.defWindow`, `d.defHitEnd`, `tickCount` all
exist (pose.cpp:736-791, animation.h:68-70).
