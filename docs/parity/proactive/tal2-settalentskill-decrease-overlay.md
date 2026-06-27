# tal2 — SetTalentSkill recomputes combat-overlay on skill *decrease* (original skips it)

**Confidence:** Medium (divergence verified on both sides) / **DEFERRED** as a fix
(negligible reachability + the obvious "mirror" patch regresses an unrelated path).

## Original function + address

`oCNpc::SetTalentSkill(int talentIdx, int newValue)` — Gothic2.exe `0x00730f60`.

The original reads the old skill value (talent object `+0x28`), writes `newValue`,
and then branches on the *sign of the change*:

- If `newValue == oldValue`: returns immediately (no work).
- If `newValue < oldValue` (a **decrease**): the original returns immediately for
  **every combat talent** (1H/2H/BOW/CROSSBOW). The *only* decrease it acts on is the
  ACROBAT talent (idx `0xB`), where it removes the hard-coded `HUMANS_ACROBATIC.MDS`
  overlay, re-runs `oCAniCtrl_Human::InitAnimations`, and halves the per-NPC
  fall-down height. So lowering a combat skill leaves the previously-applied
  weapon-stance MDS overlay (`..._1HST2.MDS`, etc.) **in place** — the visible stance
  does not drop back a tier.
- If `newValue > oldValue` (an **increase**): it removes the previous-tier overlay,
  applies the new-tier overlay (`<scheme>_<weaponhit><tier>.MDS`, where weaponhit comes
  from `oCAniCtrl_Human::GetWeaponHitString` @`0x006aeef0`: 1H→"1HS", 2H→"2HS",
  BOW→"BOW", CROSSBOW→"CBOW"), and refreshes fight anims via `InitFightAnis`/`SetFightAnis`.

Net: in the original the combat-stance overlays are **monotonic on increase only**;
a script-driven decrease never strips them.

## OpenGothic file:line

`game/world/objects/npc.cpp:1205` `Npc::setTalentSkill` →
`game/world/objects/npc.cpp:1136` `Npc::invalidateTalentOverlays(Talent)`.

```cpp
void Npc::setTalentSkill(Talent t, int32_t lvl) {
  if(t>=TALENT_MAX_G2)
    return;
  talentsSk[t] = lvl;
  invalidateTalentOverlays(t);   // unconditional, regardless of increase/decrease/equal
  }
```

`invalidateTalentOverlays(t)` recomputes the overlay set **from the new level**:
for a combat talent it `del`s the wrong-tier MDS and `add`s the right-tier MDS
for `lvl==0/1/2`.

## Divergence

OpenGothic recomputes overlays on **every** `setTalentSkill`, including a *decrease*.
So `Npc_SetTalentSkill(hero, NPC_TALENT_1H, 1)` while currently at level 2 makes
OpenGothic strip `..._1HST2.MDS` and apply `..._1HST1.MDS` (stance visibly drops to
tier 1), whereas the original leaves the tier-2 overlay applied. (Symmetrically, OG
also re-resolves on the `newValue==oldValue` case.) Verified by reading both the OG
handler and the decompiled original; symbols `talentsSk`, `invalidateTalentOverlays`,
`TALENT_1H..TALENT_CROSSBOW`, `TALENT_ACROBAT`, `addOverlay/delOverlay` all exist in OG.

## Proposed patch — DEFERRED

The naïve mirror is to gate the overlay recompute on an *increase*:

```cpp
void Npc::setTalentSkill(Talent t, int32_t lvl) {
  if(t>=TALENT_MAX_G2)
    return;
  const int32_t prev = talentsSk[t];
  talentsSk[t] = lvl;
  if(lvl>prev)                       // original only re-overlays on increase ...
    invalidateTalentOverlays(t);
  }
```

**Why DEFERRED:**
1. **Regression risk.** `Npc::setTalentSkill` is deliberately reused by OpenGothic as
   an *overlay-restore* primitive: `Npc::transformBack` (`npc.cpp:4848-4849`) calls
   `setTalentSkill(i, talentsSk[i])` with the **unchanged** stored value purely to
   re-apply weapon overlays after a visual swap, and `setVisual` (`npc.cpp:896-898`)
   does the same via `invalidateTalentOverlays()`. An `lvl>prev` gate makes that
   restore a no-op (`lvl==prev`), so transform-back would lose its combat-stance
   overlays — a worse, more reachable bug than the one being fixed.
2. **Negligible reachability.** Gothic II scripts only ever *raise* combat talents
   (`B_TeachAttributePoints`/learn flow); no shipped Daedalus path lowers a combat
   `NPC_TALENT_*` skill, so the diverging branch is essentially never hit in normal play.
3. **Direction is arguably benign.** The original's "keep the higher-tier stance after
   a decrease" is itself a quirk; OG's recompute is the more intuitive behavior.

A faithful fix would require separating the "value changed by script" entry point
(increase-gated, ACROBAT-decrease special-case) from the "re-apply overlays at current
level" entry point (used by transformBack/setVisual) — a refactor, not a surgical edit.
Holding until a parity case actually exercises a combat-skill decrease.

<!-- NOTE: in original-game oCNpc::SetTalentSkill (Gothic2.exe 0x00730f60) the combat-talent
     overlay/fight-anim work runs only when newValue>oldValue; a decrease returns early
     (only ACROBAT @idx 0xB acts on decrease). OpenGothic's setTalentSkill recomputes
     overlays unconditionally. DEFERRED: the increase-only gate breaks Npc::transformBack /
     setVisual overlay restoration, and a combat-skill decrease is unreachable from stock scripts. -->
