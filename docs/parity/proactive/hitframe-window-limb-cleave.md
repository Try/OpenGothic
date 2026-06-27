# Melee hit registration: single-frame focus-only hit vs. windowed weapon-limb cleave

**Confidence:** High (divergence) / DEFERRED (fix not surgical)

## Original function + address (prose only)

In `Gothic2.exe` the melee hit is not a single instant against one latched
enemy — it is a **frame window** during which the swinging weapon's bones are
tested for real model collision against every nearby vob:

- `oCAniCtrl_Human::GetFightLimbs` @0x006af1e0 parses three `*eventTag` kinds
  out of the active fight animation: `DEF_HIT_LIMB` (the up-to-four weapon/limb
  node names whose model collision counts as a strike, cached in the ctrl at
  this+0x13c..0x148), `DEF_OPT_FRAME` (the per-combo-step "optimal" frame, the
  END of the hit window, cached at this+0x1b4 + step*0x18 + 0x18), and
  `DEF_HIT_END`.
- `oCAniCtrl_Human::SetComboHitTarget` @0x006b0120 latches the focus enemy
  (this+0x1c4) once, at swing start (called from `oCNpc::DoAI` @0x0069c138 /
  0x0069c22e).
- `oCAniCtrl_Human::HitCombo` @0x006b0260 runs every tick while the "hit window
  active" flag (this+0x1b0 bit 0x04) is set. While the active frame is *before*
  the `DEF_OPT_FRAME` value it calls `CheckHitTarget` each tick; once the active
  frame reaches `DEF_OPT_FRAME` it clears the window flag and performs the
  guaranteed *fallback* strike on the latched focus target, gated on
  `oCNpc::IsInFightRange && oCNpc::IsInFightFocus && oCNpc::IsSameHeight`
  (and the `CanParade` @0x006b15b0 / DAM_FLY check already documented elsewhere).
- `oCAniCtrl_Human::CheckHitTarget` @0x006b05c0 walks the cached `DEF_HIT_LIMB`
  nodes, calls `zCVob::CheckModelLimbCollision` for each, and for every colliding
  vob that is `IsInFightRange` (with the "can-hit" flag this+0x1b0 bit 0x10 set)
  calls `CreateHit` on it. It sets the once-per-swing guard (this+0x1b0 bit 0x08)
  so a given swing damages each victim at most once, but it damages **all**
  victims the blade sweeps through, not only the focus target.
- `oCAniCtrl_Human::CreateHit` @0x006b0830 applies the actual damage to the vob
  it is handed (focus or swept-through).

Net original behavior: a swing damages any NPC its weapon bones physically pass
through, in fight-range, at the moment of contact anywhere inside
[hit-window-start .. DEF_OPT_FRAME]; the focus target is additionally guaranteed
a hit at DEF_OPT_FRAME. This yields multi-target cleave, hits on non-focused
enemies, and "contact wins even if the victim steps out before the opt frame."

## OpenGothic file:line

- `game/world/objects/npc.cpp:2428-2429` — `if(ev.def_opt_frame>0) commitDamage();`
  (fires once, only on the single `DEF_OPT_FRAME`/`OPTIMAL_FRAME` event crossing,
  via `tickAnimationTags`).
- `game/world/objects/npc.cpp:2051-2059` — `Npc::commitDamage()` damages **only**
  `currentTarget`, guarded by `fghAlgo.isInAttackRange` + `fghAlgo.isInFocusAngle`.
- `game/world/objects/npc.cpp:2374-2385` — `DEF_HIT_LIMB`/`HIT_END` MdsEvent cases
  are empty `break;` no-ops; OpenGothic has no weapon-limb model-collision path
  (grep: no `CheckModelLimb*`, `cleave`, `limbColl`, `weaponTrail`, `sweep`,
  `hitWindow` anywhere under `game/`).

## Divergence

OpenGothic resolves a melee strike as a **single-frame, single-target** event:
exactly at the `DEF_OPT_FRAME` event it hits the one latched `currentTarget` if
that target happens to be in range and inside the 30° focus cone at that instant.
The original resolves it as a **multi-frame weapon-bone collision window** that
damages every fight-range NPC the blade sweeps through (cleave), regardless of
whether they are the current focus, with a per-victim once-per-swing guard, and
only falls back to the focus target at the closing `DEF_OPT_FRAME`.

Observable consequences in OpenGothic:
1. One swing into a group damages at most one enemy (no cleave).
2. An enemy that is not the player's current focus/target takes no melee damage
   even if the blade clearly passes through it.
3. An enemy struck early in the swing arc who then steps out of the cone/range
   before the exact `DEF_OPT_FRAME` escapes damage (the contact does not "stick").

For the common case (one focused enemy who stays in front) OpenGothic and the
original agree, because the original's focus-target fallback also lands at
`DEF_OPT_FRAME`; the divergence is specifically the missing in-window cleave /
non-focus hits.

## Proposed patch

DEFERRED.

Reason: a faithful fix is not surgical. It requires the whole hit-detection
window subsystem that OpenGothic does not have: parsing/caching the
`DEF_HIT_LIMB` bone set, a per-tick weapon-bone-vs-vob model-collision test
across [window-start .. DEF_OPT_FRAME] (no equivalent of
`zCVob::CheckModelLimbCollision` exists in OG), and a per-victim once-per-swing
guard. Approximating cleave by simply iterating nearby NPCs in
range+focus-angle at `DEF_OPT_FRAME` and damaging all of them is **feel-tuning
with false-positive risk** (it would damage enemies the blade never reached and
could clip allies/bystanders the real bone test would have missed), which the
parity rules exclude. Recording as a known, well-localized divergence pending a
limb-collision implementation rather than shipping an approximation.
