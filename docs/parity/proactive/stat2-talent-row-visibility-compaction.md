# STATS screen: talent rows not filtered for "learned" and not compacted

**Confidence:** High (divergence); High (patch)

## Original function + address
- `oCMenu_Status::InitForDisplay` @ `0x0047ddc0` — the third loop of this function
  walks every talent that `oCNpc::OpenScreen_Status` registered via `AddTalent`
  and writes them into the menu rows `MENU_ITEM_TALENT_<n>`.
- `oCNpc::OpenScreen_Status` @ `0x0073de12` — builds the per-talent records
  (`oSMenuInfoTalent`, stride `0x44`): title from `TXT_TALENTS[i]`, skill-string
  from `TXT_TALENTS_SKILLS[i]`, the numeric **skill** value (`oCNpcTalent+0x28`,
  i.e. `GetTalentSkill` @ `0x007317f0`) and the **value/percent** (`oCNpcTalent+0x2c`,
  i.e. `GetTalentValue` @ `0x00730dc0`, or the hitchance for the 4 combat talents).

Two behaviours of the original loop matter:

1. **Visibility filter.** For each talent it tests
   `(talentSkill > 0) || (TXT_TALENTS_SKILLS[i] != "")`. A talent whose skill
   value is 0 **and** whose `TXT_TALENTS_SKILLS` entry is empty is skipped
   entirely — it is never written to any menu row. This is the
   "only show learned talents" behaviour of the character screen.

2. **Compaction.** The row counter (`local_204`, 1-based) advances **only** when a
   talent passes the visibility test. So the n-th *visible* talent is written to
   `MENU_ITEM_TALENT_<n>`. The slot number is the visible rank, not the talent id.
   Trailing/unused rows keep their (empty) `.DAT` defaults.

## OpenGothic file:line
`game/ui/gamemenu.cpp:1248-1265` (`GameMenu::setPlayer`, the talent loop).

## Divergence
OpenGothic writes each named talent to a slot indexed by the **talent id**
(`MENU_ITEM_TALENT_<i>`, i = 1..21) and applies **no** learned/visibility filter —
it only skips talents whose *name* (`TXT_TALENTS[i]`) is empty:

```cpp
for(uint16_t i=0; i<talentMax; ++i) {
  auto& str = tal->get_string(i);
  if(str.empty())
    continue;
  ...
  set(string_frm("MENU_ITEM_TALENT_",i,"_TITLE"), str);   // slot == talent id
  ...
}
```

Consequences versus the original:
- Unlearned talents (skill 0 and empty skill-string) are displayed instead of
  hidden, so the character screen shows rows the original suppresses.
- Because the slot equals the talent id (with gaps — id 6 is unused, ids run to
  21) and is never compacted, visible talents land in non-consecutive rows, and
  any high-id talent whose id exceeds the number of `MENU_ITEM_TALENT_*` slots the
  `.DAT` defines is written to a non-existent item (silently dropped) instead of
  being packed into an existing low row as the original does.

## Proposed patch
Add the visibility test and a compacted 1-based row counter (`row`) so the n-th
visible talent goes to `MENU_ITEM_TALENT_<row>`, matching `InitForDisplay`.
All referenced symbols are grep-verified: `pl.talentSkill(Talent)` (npc.h:202),
`talV->get_string(i)` returns `std::string const&` (`.empty()` valid), `string_frm`
already used here with an integer first row argument.

OLD (`game/ui/gamemenu.cpp:1246-1265`):
```cpp
  const bool g2        = Gothic::inst().version().game==2;
  const int  talentMax = g2 ? TALENT_MAX_G2 : TALENT_MAX_G1;
  for(uint16_t i=0; i<talentMax; ++i) {
    auto& str = tal->get_string(i);
    if(str.empty())
      continue;

    const int sk  = pl.talentSkill(Talent(i));
    // NOTE: in original-game OpenScreen_Status @0x0073de12 the talent "%" column is the per-NPC
    // hitchance ONLY for the four combat talents (1H/2H/BOW/CBOW, ids 1..4); every other talent
    // row shows the stored talent value (oCNpc::SetTalentValue), not hitchance -- OpenGothic used
    // hitchance for all G2 talents, so non-combat rows displayed 0.
    const bool combat = (Talent(i)==TALENT_1H  || Talent(i)==TALENT_2H ||
                         Talent(i)==TALENT_BOW || Talent(i)==TALENT_CROSSBOW);
    const int val = (g2 && combat) ? pl.hitChance(Talent(i)) : pl.talentValue(Talent(i));

    set(string_frm("MENU_ITEM_TALENT_",i,"_TITLE"), str);
    set(string_frm("MENU_ITEM_TALENT_",i,"_SKILL"), strEnum(talV->get_string(i),sk,textBuf));
    set(string_frm("MENU_ITEM_TALENT_",i),          string_frm(val,"%"));
    }
```

NEW:
```cpp
  const bool g2        = Gothic::inst().version().game==2;
  const int  talentMax = g2 ? TALENT_MAX_G2 : TALENT_MAX_G1;
  int        row       = 0;
  for(uint16_t i=0; i<talentMax; ++i) {
    auto& str = tal->get_string(i);
    if(str.empty())
      continue;

    const int sk = pl.talentSkill(Talent(i));
    // NOTE: in original-game oCMenu_Status::InitForDisplay @0x0047ddc0 a talent is shown only when
    // talentSkill>0 OR its TXT_TALENTS_SKILLS entry is non-empty, and visible talents are packed
    // into consecutive MENU_ITEM_TALENT_<n> rows (1-based, n = visible rank). OpenGothic mapped
    // slot==talentId and showed every named talent, so unlearned talents appeared and high-id
    // talents could fall outside the menu's row set.
    if(sk<=0 && talV->get_string(i).empty())
      continue;
    ++row;

    // NOTE: in original-game OpenScreen_Status @0x0073de12 the talent "%" column is the per-NPC
    // hitchance ONLY for the four combat talents (1H/2H/BOW/CBOW, ids 1..4); every other talent
    // row shows the stored talent value (oCNpc::SetTalentValue), not hitchance -- OpenGothic used
    // hitchance for all G2 talents, so non-combat rows displayed 0.
    const bool combat = (Talent(i)==TALENT_1H  || Talent(i)==TALENT_2H ||
                         Talent(i)==TALENT_BOW || Talent(i)==TALENT_CROSSBOW);
    const int val = (g2 && combat) ? pl.hitChance(Talent(i)) : pl.talentValue(Talent(i));

    set(string_frm("MENU_ITEM_TALENT_",row,"_TITLE"), str);
    set(string_frm("MENU_ITEM_TALENT_",row,"_SKILL"), strEnum(talV->get_string(i),sk,textBuf));
    set(string_frm("MENU_ITEM_TALENT_",row),          string_frm(val,"%"));
    }
```
