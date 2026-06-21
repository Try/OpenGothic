# Talent stats-menu "%" column uses hitchance for ALL talents instead of only the 4 combat skills

**Confidence:** High

## Original function + address

In `Gothic2.exe`, the character stats screen ("Talents & Skills" page) is built by
`OpenScreen_Status` (`0x0073de12`), which iterates over each talent index, calls
`oCNpc::GetTalentInfo` (`0x0073c860`) to fill the row title / skill-name strings, and
then fills the two numeric columns of the per-row `oSMenuInfoTalent` struct as follows:

- The "skill tier" field (struct offset `+0x40`) is set to the talent object's *skill*
  value (`talentObj + 0x28`, i.e. the value written by `oCNpc::SetTalentSkill`,
  `0x00730f60` — the 0/1/2 weapon tier).
- The "value / percent" field (struct offset `+0x3c`) is selected by a `switch` on the
  talent **id** (`talentObj + 0x24`):
  - id `1` (1H)  -> `this+0x1dc`
  - id `2` (2H)  -> `this+0x1e0`
  - id `3` (BOW) -> `this+0x1e4`
  - id `4` (CROSSBOW) -> `this+0x1e8`
  - **default** (everything else, including id `0` and id `>=5` such as PICKLOCK,
    SNEAK, ACROBAT, PICKPOCKET, SMITH, RUNES, ALCHEMY, ...) -> `talentObj + 0x2c`,
    i.e. the talent **value** written by `oCNpc::SetTalentValue` (`0x00730be0`).

The four consecutive ints at `this+0x1dc..0x1e8` are the per-NPC hitchance array for the
four combat talents (1H/2H/BOW/CBOW). So in the original, the "%" column shows the
*hitchance* only for the four combat talents, and the stored *talent value* for every
other talent row.

(The id-validity bound in `Get/SetTalentSkill` / `Get/SetTalentValue` is `0..0x16`, but
the live talent list `oCNpcTalent::CreateTalentList` (`0x0072c670`) allocates exactly
`0x16`=22 entries, so the effective read/write range is id `0..21`. This matches OG's
`TALENT_MAX_G2 = 22` — checked and found to be NOT a divergence.)

## OpenGothic file:line

`game/ui/gamemenu.cpp:1254`

```cpp
const int sk  = pl.talentSkill(Talent(i));
const int val = g2 ? pl.hitChance(Talent(i)) : pl.talentValue(Talent(i));
```

## Divergence

For Gothic 2, OpenGothic uses `pl.hitChance(Talent(i))` for the "%" column of **every**
talent row. The original uses hitchance only for the four combat talents (ids 1..4) and
`talentValue(i)` for all other rows.

`Npc::hitChance` (`game/world/objects/npc.cpp:1212`) returns `hnpc->hitchance[t]` for
`t < hitchance_count` (5) and `0` otherwise. Consequences vs. the original:

- Non-combat talent rows that the original fills from `SetTalentValue` (e.g. SNEAK,
  PICKPOCKET, ACROBAT, SMITH, RUNES, ALCHEMY, TAKEANIMALTROPHY, ...; all ids `>=5`)
  display `hitChance(i) == 0` in OpenGothic, instead of their stored talent value.
  In stock G2 most of these rows have no value, but any mod that uses
  `Npc_SetTalentValue` to show a sneak/pickpocket/etc. percentage will read 0 in OG.
- Talent id `0` (`TALENT_UNKNOWN`) and id `5` (`TALENT_PICKLOCK`): the original uses the
  `default` branch -> talent value; OG returns `hitchance[0]` / `hitchance[5]` (the latter
  already OOB-guarded to 0 by the existing hitchance fix). Either way the source array is
  wrong relative to the original for these ids.

The skill-tier column (`sk`) and the empty-row filter are already correct; only the value
column selection diverges.

## Proposed patch

OLD (`game/ui/gamemenu.cpp:1253-1254`):
```cpp
    const int sk  = pl.talentSkill(Talent(i));
    const int val = g2 ? pl.hitChance(Talent(i)) : pl.talentValue(Talent(i));
```

NEW:
```cpp
    const int sk  = pl.talentSkill(Talent(i));
    // NOTE: in original-game OpenScreen_Status @0x0073de12 the talent "%" column is the
    // per-NPC hitchance ONLY for the four combat talents (1H/2H/BOW/CBOW, ids 1..4);
    // every other talent row shows oCNpc::SetTalentValue (talentObj+0x2c), not hitchance.
    const bool combat = (Talent(i)==TALENT_1H || Talent(i)==TALENT_2H ||
                         Talent(i)==TALENT_BOW || Talent(i)==TALENT_CROSSBOW);
    const int val = (g2 && combat) ? pl.hitChance(Talent(i)) : pl.talentValue(Talent(i));
```

Grep-verified OG symbols used: `Npc::talentSkill`, `Npc::talentValue`, `Npc::hitChance`
(`game/world/objects/npc.h:202,208,209`); `TALENT_1H/2H/BOW/CROSSBOW`
(`game/game/constants.h:447-450`).
