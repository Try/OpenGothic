# Fight-tactic: mage `my_fk_focus_mag` selected by weapon-range instead of focus-angle

**Confidence:** High

## Original function + address

`oCNpc::FindNextFightAction` (Gothic2.exe `0x0067d680`), in the magic-weapon branch
(reached when `GetWeaponMode()` returns the magic mode, value 7).

In that branch the original does **not** test any distance/range value. It calls the
fight-focus test (`oCNpc::IsInFightFocus(this, target)` — the aiming-cone test, the same
primitive OpenGothic calls `isInFocusAngle`). When the NPC has the target in fight focus it
selects the `FA_MY_FK_FOCUS_MAG_<tactic>` block (struct offset `+0x1dc`); otherwise it
selects `FA_MY_FK_NOFOCUS_MAG_<tactic>` (struct offset `+0x1f8`). The bow branch of the same
function behaves identically (focus -> `..._FOCUS_FAR`, else `..._NOFOCUS_FAR`), confirming
the ranged-weapon tables are gated on focus, never on melee weapon-range.

(Cross-checked against `FUN_0067ce70`, the target-situation getter, and against the
`InitFightAI`/`InitBlock` instance-name table `FA_<PREFIX>_<id>`, whose magic prefixes are
`MY_FK_FOCUS_MAG` / `MY_FK_NOFOCUS_MAG` — matching OpenGothic's `my_fk_focus_mag` /
`my_fk_nofocus_mag` members, so this is purely a gate-condition divergence, not a name
mismapping.)

## OpenGothic file:line

`game/game/fightalgo.cpp:86-92` (the `ws==WeaponState::Mage` branch in `FightAlgo::fillQueue`).

## Divergence

OpenGothic gates the focused mage move-table on **weapon-range** rather than focus-angle:

```cpp
if(ws==WeaponState::Mage) {
  if(isInWRange(npc,tg,owner))                 // <-- wrong gate
    if(fillQueue(owner,ai.my_fk_focus_mag))
      return;
  if(fillQueue(owner,ai.my_fk_nofocus_mag))
    return;
  }
```

`isInWRange` is the melee "weapon range" check (≈ `FIGHT_RANGE_FIST*3`). A spell-casting NPC
fights from a distance and is essentially never inside that melee range, so OpenGothic falls
through to `my_fk_nofocus_mag` on virtually every tick and almost never plays the
`my_fk_focus_mag` table — even when the mage is correctly aimed at its target. The original
selects `my_fk_focus_mag` purely on having the target in the aiming cone (`focus`), which is
the relevant condition for a ranged caster.

## Proposed patch

The `focus` variable (`isInFocusAngle(npc,tg)`) is already computed at line 43 and is the
exact OpenGothic equivalent of the original's `IsInFightFocus(this,target)`.

OLD (`game/game/fightalgo.cpp:86-92`):
```cpp
  if(ws==WeaponState::Mage) {
    if(isInWRange(npc,tg,owner))
      if(fillQueue(owner,ai.my_fk_focus_mag))
        return;
    if(fillQueue(owner,ai.my_fk_nofocus_mag))
      return;
    }
```

NEW:
```cpp
  if(ws==WeaponState::Mage) {
    // NOTE: in original-game oCNpc::FindNextFightAction @0x0067d680 the magic branch gates
    // my_fk_focus_mag on IsInFightFocus (aiming cone), not on melee weapon-range; a caster is
    // almost never inside W-range, so the range gate made the focused-spell table unreachable.
    if(focus)
      if(fillQueue(owner,ai.my_fk_focus_mag))
        return;
    if(fillQueue(owner,ai.my_fk_nofocus_mag))
      return;
    }
```

Grep-verified symbols: `focus` (fightalgo.cpp:43), `isInFocusAngle` (fightalgo.h:58),
`my_fk_focus_mag` / `my_fk_nofocus_mag` (definitions/fightaidefinitions.h),
`WeaponState::Mage`.
