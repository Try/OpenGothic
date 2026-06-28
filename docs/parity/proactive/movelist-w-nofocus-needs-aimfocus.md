# Fight move-list: `my_w_nofocus` fires without aim-focus (missing `my_g_fk_nofocus` catch-all)

**Confidence:** Medium-High (binary gate is unambiguous; residual risk only on whether the
target catch-all table `FA_MY_G_FK_NOFOCUS_<id>` is populated in stock G2 scripts).

## Original fn + address

`oCNpc::FindNextFightAction` @ `Gothic2.exe 0x0067d680` (`oNpc_Fight.cpp`). For a melee/ranged
NPC (`GetWeaponMode() < 5`) it walks a fixed priority list of move tables (switch on `local_9c`,
cases 0..0xe) and returns the first table that is non-empty for the current state. The candidate
tables live at fixed offsets inside the per-tactic FightAi struct (`DAT_00aac678[fight_tactic]`),
each table being `move[6]+count` = 0x1c bytes, so the offsets map 1:1 to the named tables loaded
by OpenGothic (`fightaidefinitions.cpp`):

- case 5 = `my_w_focus`  (offset 0x8c) — gate `local_b4 && local_c0 && my_anim==5`
- case 6 = `my_w_nofocus`(offset 0xa8) — gate **`local_b4 && local_c0`**
- case 7 = `my_g_combo`  (offset 0xc4) — gate `local_b4`
- case 0xb = `my_fk_focus`     (offset 0x134) — gate `local_b8 && local_c0`
- case 0xc = `my_g_fk_nofocus` (offset 0x150) — gate **`local_b8`** (no focus)

`local_c0 = IsInFightFocus(this,target)` is the ~30-degree aim cone — i.e. exactly OpenGothic's
`focus = isInFocusAngle(npc,tg)` (the same equivalence already cited in the existing fillQueue
NOTEs). Crucially, **all four `my_w_*` attack tables (cases 3-6) require `local_c0`**; the
"focus/nofocus" in their names distinguishes the NPC's own combat *animation phase*
(`local_bc`, from `FUN_0067ce70`), not whether it has the target in its aim cone. When the NPC is
in weapon range but the target is OUTSIDE the aim cone (`local_c0 == 0`), every `my_w_*`/`my_g_*`
attack band is skipped and the list falls through to `my_g_fk_nofocus` (case 0xc, gated on range
only), which in G2 holds turn/reposition moves that re-orient the NPC toward the target.

## OG file:line

`/Users/admin/Downloads/opengothic/game/game/fightalgo.cpp:64-74` (melee W-range block).
Note also: `my_g_fk_nofocus` is loaded into the FA struct (`fightaidefinitions.cpp`) but is
**never referenced** anywhere in `fightalgo.cpp`.

## Divergence

```cpp
if(isInWRange(npc,tg)) {
  if(focus && npc.bodyStateMasked()==BS_RUN)  ... my_w_runto
  if(focus && npc.bodyStateMasked()!=BS_RUN)  ... my_w_focus
  if(fillQueue(owner,ai.my_w_nofocus)) return;   // <-- fired even when !focus
  }
```

`ai.my_w_nofocus` is rolled unconditionally inside the W-range block. In the original it is gated
on `local_c0` (aim-focus). Effect: an NPC whose target is in melee range but >30 degrees off its
facing performs a blind `my_w_nofocus` swing, whereas the original does NOT attack — it selects
`my_g_fk_nofocus` (turn-toward / reposition) and only swings once it has rotated the target back
into its aim cone. This makes OpenGothic melee NPCs land "sideways" hits that the original
engine refuses, and it leaves the loaded `my_g_fk_nofocus` table dead.

## Proposed patch

```cpp
// OLD (fightalgo.cpp, inside `if(isInWRange(npc,tg))`)
      if(focus && npc.bodyStateMasked()==BS_RUN)
        if(fillQueue(owner,ai.my_w_runto))
          return;
      if(focus && npc.bodyStateMasked()!=BS_RUN)
        if(fillQueue(owner,ai.my_w_focus))
          return;
      if(fillQueue(owner,ai.my_w_nofocus))
        return;
      }

// NEW
      // NOTE: in original-game oCNpc::FindNextFightAction @0x0067d680 every my_w_* attack table
      // (cases 3-6) is gated on IsInFightFocus (the aim cone, == `focus`); my_w_nofocus (case 6,
      // struct offset 0xa8) requires `local_b4 && local_c0` -- "nofocus" there means the NPC's own
      // combat-animation phase, NOT a missing aim cone. When in range but the target is outside the
      // aim cone the original falls through to my_g_fk_nofocus (case 0xc, offset 0x150, gated on
      // range only), a turn/reposition table. OpenGothic fired my_w_nofocus without the aim-focus
      // gate, so NPCs blind-attacked targets >30deg off their facing instead of turning to face.
      if(focus && npc.bodyStateMasked()==BS_RUN)
        if(fillQueue(owner,ai.my_w_runto))
          return;
      if(focus && npc.bodyStateMasked()!=BS_RUN)
        if(fillQueue(owner,ai.my_w_focus))
          return;
      if(focus) {
        if(fillQueue(owner,ai.my_w_nofocus))
          return;
        }
      else {
        if(fillQueue(owner,ai.my_g_fk_nofocus))
          return;
        }
      }
```

`ai.my_g_fk_nofocus`, `ai.my_w_nofocus`, and `focus` all already exist, so the change is
build-verifiable. Residual risk: if `FA_MY_G_FK_NOFOCUS_<id>` is empty for a given tactic the
not-focused path falls through to the later `my_fk_*` bands (current behavior for that sub-case),
which is still closer to the original than an unconditional `my_w_nofocus` swing.
