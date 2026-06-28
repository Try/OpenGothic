# overlay — ACROBAT overlay name is hardcoded "HUMANS_ACROBATIC.MDS" in the original

**Confidence:** Medium that the divergence is real (verified on both sides) /
**DEFERRED** as a fix (effectively unreachable on stock content; a "faithful" hardcode
would regress modded non-human acrobatic NPCs).

## Original function + address

`oCNpc::SetTalentSkill(int talentIdx, int newValue)` — Gothic2.exe `0x00730f60`.

For the ACROBAT talent (index `0xB`) the original builds the overlay-MDS name from a
single hard-coded string literal at `0x008b8794` = `"HUMANS_ACROBATIC.MDS"`, used in
**both** the apply and the remove path:

- On an *increase* (`newValue > oldValue`, switch `case 0xB`): it `ApplyOverlay`s the
  literal `"HUMANS_ACROBATIC.MDS"`, re-runs `oCAniCtrl_Human::InitAnimations`, and
  doubles the per-NPC fall-down height.
- On a *decrease* (`newValue < oldValue`, the only decrease branch that acts): it
  removes that same literal `"HUMANS_ACROBATIC.MDS"` via `oCNpc::RemoveOverlay`
  (`0x0072d5c0`) and halves the fall-down height.

This is the **only** talent overlay the original hardcodes. The weapon overlays
(1H/2H/BOW/CROSSBOW) are built dynamically from the loaded model-prototype's base
name (e.g. `"HUMANS"`) concatenated with `oCAniCtrl_Human::GetWeaponHitString`
(`0x006aeef0`: 1H→"1HS", 2H→"2HS", BOW→"BOW", CROSSBOW→"CBOW"), the tier digit, and
`.MDS` — i.e. `<modelScheme>_<weaponhit><tier>.MDS`. So the original is *scheme-relative
for weapons but scheme-absolute (always "HUMANS") for acrobatics*.

## OpenGothic file:line

`game/world/objects/npc.cpp:1225-1229` — `Npc::invalidateTalentOverlays(Talent)`:

```cpp
else if(t==TALENT_ACROBAT){
  if(lvl==0)
    delOverlay(string_frm(scheme,"_ACROBATIC.MDS")); else
    addOverlay(string_frm(scheme,"_ACROBATIC.MDS"),0);
  }
```

where `scheme = visual.visualSkeletonScheme()` (`game/graphics/mdlvisual.cpp:909`) is the
NPC's own skeleton base name (`skeleton->name()` truncated at the first `.`/`_`).

## Divergence

OpenGothic constructs the acrobat overlay name *relative to the NPC's own skeleton
scheme* (`<scheme>_ACROBATIC.MDS`), exactly as it does for the weapon overlays. The
original instead always uses the fixed literal `"HUMANS_ACROBATIC.MDS"` for acrobatics
regardless of the NPC's skeleton.

- For the player and all human NPCs `scheme == "HUMANS"`, so the two produce the
  identical name `"HUMANS_ACROBATIC.MDS"` — behavior is bit-for-bit equal.
- They diverge only for an NPC whose skeleton scheme is **not** `HUMANS` (e.g. an
  `ORC`/monster skeleton) that is given `NPC_TALENT_ACROBAT`: OpenGothic looks for
  `"<scheme>_ACROBATIC.MDS"` (e.g. `"ORC_ACROBATIC.MDS"`, which does not exist as a
  shipped MDS) whereas the original applies `"HUMANS_ACROBATIC.MDS"`.

Verified by reading both sides: the literal at `0x008b8794`, the `case 0xB` apply, the
acrobat-decrease remove, and OG's `invalidateTalentOverlays`/`visualSkeletonScheme`.
Symbols `TALENT_ACROBAT`, `visualSkeletonScheme`, `string_frm`, `addOverlay/delOverlay`
all exist in OG.

## Proposed patch — DEFERRED

A literal "match the original" change would be:

```cpp
// game/world/objects/npc.cpp  Npc::invalidateTalentOverlays(Talent)
// OLD:
  else if(t==TALENT_ACROBAT){
    if(lvl==0)
      delOverlay(string_frm(scheme,"_ACROBATIC.MDS")); else
      addOverlay(string_frm(scheme,"_ACROBATIC.MDS"),0);
    }

// NEW:
  else if(t==TALENT_ACROBAT){
    // NOTE: in original-game oCNpc::SetTalentSkill (Gothic2.exe 0x00730f60) the ACROBAT
    // overlay name is the hard-coded literal "HUMANS_ACROBATIC.MDS" (string @0x008b8794),
    // applied/removed regardless of the NPC's skeleton scheme — unlike the weapon
    // overlays, which the original builds scheme-relative.
    if(lvl==0)
      delOverlay("HUMANS_ACROBATIC.MDS"); else
      addOverlay("HUMANS_ACROBATIC.MDS",0);
    }
```

**Why DEFERRED:**
1. **Effectively unreachable on stock content.** In Gothic II `NPC_TALENT_ACROBAT` is
   only ever granted to the (human) player; no shipped Daedalus path gives acrobatics to
   a non-human NPC, so `scheme` is always `"HUMANS"` and the two formulations are
   identical. There is no observable in-game difference on retail content.
2. **The original's literal is itself arguably the buggy side.** Hardcoding `"HUMANS"`
   means a (hypothetical/modded) non-human acrobat would get the wrong, human overlay;
   OpenGothic's scheme-relative form is the more internally consistent behavior and would
   correctly pick up a mod-supplied `<scheme>_ACROBATIC.MDS`. "Fixing" toward the literal
   would regress that modded case for zero stock-content benefit.

Holding until a parity case actually exercises ACROBAT on a non-`HUMANS` skeleton.

<!-- NOTE: in original-game oCNpc::SetTalentSkill (Gothic2.exe 0x00730f60) the ACROBAT
     overlay uses the hard-coded literal "HUMANS_ACROBATIC.MDS" (@0x008b8794) for both
     apply and remove, whereas weapon overlays are built scheme-relative. OpenGothic
     (npc.cpp:1225 invalidateTalentOverlays) builds <scheme>_ACROBATIC.MDS. Identical for
     human NPCs (scheme=="HUMANS"); diverges only for non-human acrobats. DEFERRED:
     unreachable on stock content and the scheme-relative form is the better behavior. -->
