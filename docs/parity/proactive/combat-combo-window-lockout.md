# Combo lockout on a mistimed same-direction attack press

**Confidence:** Medium-High (root cause is exact and grep-verified; matches original control flow). Build-verifiable, surgical.

## Original function + address

`oCAniCtrl_Human::HitCombo` (Gothic2.exe `0x006b0260`), supported by the combo-struct
parser `oCAniCtrl_Human::GetFightLimbs` (`0x006af1e0`).

The combo state machine in `HitCombo` keeps a per-step struct (stride `0x18`) holding, per
combo index, the optimal/hit frame (`+0x1cc`, parsed from `*EVENTDEFOPTFRAME`), the hit-end
frame (`+0x1d0`, `*EVENTDEFHITEND`), and the combo-window start/end frames (`+0x1d4` / `+0x1d8`,
`*EVENTDEFWINDOW`). When the attack key is pressed again during a swing (`param_1 != 0`), the
function checks whether the current frame lies inside the combo window using an **inclusive**
test on both ends: `windowStart <= frame <= windowEnd`. If it is inside, it accelerates the
swing (`SetActFrame(windowEnd - 1)`) so the next swing chains. **If the press is OUTSIDE the
window (too early or too late), the original simply clears the transient "press pending" bit
(`flags &= ~1`) and falls through — it does NOT break or lock the combo.** A subsequent press
that does land inside the window still chains normally. There is no code path in `HitCombo`
that aborts an in-progress combo because of a mistimed press; the combo only advances at
hit-end (`+0x1d0`) or ends when the step index passes the optimal-frame count (`+0x1b8`).

(Damage timing and combo-advance reference were also checked and already match: OpenGothic
commits melee damage on the `OPTIMAL_FRAME` event via `Npc::commitDamage()` gated by
`ev.def_opt_frame>0`, exactly as `HitCombo` applies `CreateHit` when the active frame reaches
`+0x1cc` (= `DEF_OPT_FRAME`), and the per-step re-base to `defHitEnd[...]` mirrors the original
hit-end advance. Those are NOT divergent.)

## OpenGothic file:line

`game/graphics/mesh/pose.cpp:771-774` (`Pose::continueCombo`), with the boundary helper at
`game/graphics/mesh/animation.cpp:275-278` (`Animation::Sequence::isInComboWindow`).

## Divergence

When a new swing is requested whose animation name equals the currently-playing swing
(`prev->seq->name==sq->name`) but the current time is **outside** the combo window,
`continueCombo` calls `combo.setBreak()`. `setBreak()` raises the `0x8000` flag in `ComboState`,
which is only cleared by a full `combo = ComboState()` reset (i.e. a brand-new swing). For the
rest of the current swing, `if(combo.isBreak()) return nullptr;` then rejects every further
continuation, so an in-window press that arrives later is also refused. Net effect: pressing
attack a little **too early** (before the window opens) permanently kills the combo chain for
that swing, forcing the player to wait out the full swing and restart — strictly more punishing
than the original, which silently ignores the early press and lets the player retry inside the
window.

Secondary (same lines): the window low-boundary is exclusive in OpenGothic
(`defWindow[id+0] < t`) but inclusive in the original (`windowStart <= frame`). This is a one-
frame edge difference and is folded into the same patch.

## Proposed patch

```
// game/graphics/mesh/pose.cpp  (Pose::continueCombo)
OLD:
  if(!(d.defWindow[id+0]<t && t<=d.defWindow[id+1])) {
    if(prev->seq->name==sq->name && sq->data->defHitEnd.size()>0)
      combo.setBreak();
    return nullptr;
    }
NEW:
  // NOTE: in original-game oCAniCtrl_Human::HitCombo @0x006b0260 a re-press that falls outside
  // the combo window [windowStart..windowEnd] (inclusive both ends) is silently ignored: the
  // "press pending" bit is cleared but the combo is NOT broken, so a later in-window press still
  // chains. OpenGothic broke (setBreak) the combo on any same-direction mistimed press, which
  // permanently locked out the chain after a too-early press. Drop the lockout and make the low
  // bound inclusive to match the original frame test.
  if(!(d.defWindow[id+0]<=t && t<=d.defWindow[id+1]))
    return nullptr;
```

Grep-verified symbols used: `ComboState::isBreak`/`setBreak` (pose.h:112-113),
`Pose::ComboState combo` (pose.h:142), `Animation::AnimData::defWindow`/`defHitEnd`
(animation.h:68-70), `Animation::Sequence::data` (used throughout pose.cpp:748).
The `combo.setBreak()` at pose.cpp:766 (combo-count cap, `id+1>=defWindow.size()`) is left
intact — that path corresponds to the original's step-index limit (`+0x1b8`) and is correct.
