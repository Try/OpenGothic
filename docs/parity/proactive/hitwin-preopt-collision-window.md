# Melee swing hit-window: single opt-frame hit vs. original's pre-opt collision window

**Confidence:** Medium (divergence is real and well-evidenced; the corrective fix is architectural, so the fix is **DEFERRED**)

## Original function + address (prose only)

The melee active-hit logic in `Gothic2.exe` lives in `oCAniCtrl_Human`:

- `oCAniCtrl_Human::HitCombo` @ `0x006b0260` is the per-frame swing tick. While the
  swing's hit-active flag (bit `0x04`) is set it does **two distinct things depending on
  the current animation frame `f`** (per combo-step the relevant boundary field sits at
  `this + stepIdx*0x18 + 0x1cc`, i.e. the step's optimal/DEF_OPT_FRAME):
  - **`f < optFrame`** -> it calls `oCAniCtrl_Human::CheckHitTarget` @ `0x006b05c0`
    *every frame*. `CheckHitTarget` walks the 4 fight-limb node instances and calls
    `zCVob::CheckModelLimbCollision` on each, i.e. it does a real **weapon-mesh vs.
    body-limb collision test**, and on the first colliding vob that passes
    `oCNpc::IsInFightRange` @ `0x0067cb60` it fires `oCAniCtrl_Human::CreateHit`
    @ `0x006b0830` (sends the damage message).
  - **`f >= optFrame`** (one-shot, clears bit `0x04`) -> a *guaranteed* hit on the locked
    combo target (`this + 0x1c4`, set by `SetComboHitTarget`), gated by
    `IsInFightRange && IsInFightFocus (@0x00735290) && IsSameHeight (@0x00737be0)`.
- `oCAniCtrl_Human::HitGraphical` @ `0x006b11e0` is the other tick path; it likewise runs
  `CheckHitTarget` continuously, gated only by `zCModel::GetProgressPercent >= 0.3`
  (constant `0x3e99999a`). So the original's active-hit window is a **frame *range*** that
  begins early in the swing and runs to the optimal frame, polling weapon-limb collision
  the whole time, and the DEF_OPT_FRAME hit is merely the *terminal, guaranteed* hit on the
  focus target.

I verified that the two attacker-side gates OpenGothic *does* model are faithful:
`IsInFightFocus` uses a 30-degree yaw cone (`< 0x1e`) matching `FightAlgo::isInFocusAngle`'s
default `cos(30deg)`, and `IsSameHeight` (0.25 * target-height bbox-overlap tolerance) matches
`fightSameHeight` in `fightalgo.cpp:260` byte-for-byte.

## OpenGothic file:line

- `game/world/objects/npc.cpp:2452` `Npc::tickAnimationTags` -> `if(ev.def_opt_frame>0) commitDamage();`
- `game/world/objects/npc.cpp:2074` `Npc::commitDamage` (the only melee damage path)
- `game/graphics/mesh/animation.cpp:439-472` produces `ev.def_opt_frame` only when the
  `OPTIMAL_FRAME` (DEF_OPT_FRAME) MDS tag frame is crossed.

## Divergence

OpenGothic registers a melee hit **only at the single instant the `OPTIMAL_FRAME` tag is
crossed**, exclusively against `currentTarget`, via `commitDamage`. The original opens an
**active-hit window spanning the frame range from early in the swing up to the optimal
frame** and polls actual weapon-limb collision every frame in that range (`CheckHitTarget`),
treating the DEF_OPT_FRAME hit only as the terminal guaranteed strike. Consequences in OG:

1. A target that is within reach/cone *early* in the swing but has moved out (or the
   attacker re-orients) by the optimal frame is never hit, whereas the original would have
   already registered the early collision hit.
2. The original can incidentally hit a *non-`currentTarget`* vob that the weapon mesh
   physically sweeps through before the opt frame (the pre-opt path hits "whatever the blade
   touches"); OG can only ever damage `currentTarget`.

Secondary, narrower observation (separate candidate): the original's **reach test at the hit
frame** is `oCNpc::IsInFightRange` = `bbox-surface-distance <= attackerBaseRange +
weaponRange` (it does **not** add the *target's* guild `fight_range_base`). OpenGothic's
`commitDamage` reuses `FightAlgo::isInAttackRange`
(`fightalgo.cpp:318`/`prefferedAttackDistance` `fightalgo.cpp:292`), which is
`base[tg] + base[npc] + weaponRange` -- i.e. the AI *attack-decision* range, which adds the
target's base term. So OG's hit-frame reach is the looser decision range rather than the
tighter dedicated hit reach the original uses; this can let a hit land from marginally
farther than the original allowed.

## Proposed patch

**DEFERRED.**

- The primary divergence (continuous pre-opt weapon-limb **collision** window) cannot be
  reproduced surgically: OpenGothic has no `zCVob::CheckModelLimbCollision` equivalent and
  deliberately models melee as a single event-driven `commitDamage` on `currentTarget`.
  Reintroducing a frame-range collision sweep is an architectural change, not a
  high-confidence one-liner, and risks regressions in the existing (verified-correct) cone
  and height gates.
- The secondary reach observation is plausibly fixable (use a hit-specific reach that omits
  `base[tg]`, mirroring `IsInFightRange`), but `isInAttackRange` is shared with the AI's own
  "may I attack" decision and `fightDistanceTo`/`fight_range_base` were already tuned in
  prior parity work; changing the hit-frame reach in isolation could desync the attack
  trigger from the hit and is not high-confidence without empirical range measurements.
  Left as DEFERRED pending a targeted in-game reach comparison.
