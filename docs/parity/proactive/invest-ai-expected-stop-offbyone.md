# NPC invest-spell stop gate releases one mana-tick early (aiExpectedInvest off-by-one)

**Confidence:** High

## Original function + address (prose only)

`oCNpc::EV_CastSpell` @ `0x0067fb20` drives an NPC that is mid-invest. Each tick it
compares the NPC's "expected invest" field (`oCNpc+0x574`) against the spell's
mana-invested counter (`oCSpell+0x48`). The branch is taken with a **strict** less-than:
when `expected < manaInvested` it forces the invest-status result to 0, which routes
`oCAIHuman::MagicCheckSpellStates` into the release/cast path; otherwise (`expected >= manaInvested`)
it keeps the live invest status and continues investing. In other words the NPC keeps
investing while `manaInvested <= expected` and only releases once `manaInvested` has
**exceeded** `expected`.

Field provenance (verified, prose only):
- `oCNpc+0x574` is written by `oCNpc::ReadySpell` @ `0x006802e0` from its `investMax`
  parameter (the B_ReadySpell/AI invest count). This is OpenGothic's `aiExpectedInvest`.
- `oCSpell+0x48` is the per-tick mana-invested counter, initialized to 0 by
  `oCSpell::InitByScript` @ `0x00484550` and incremented in `oCSpell::Invest` @ `0x004850d0`
  on each successful invest tick. This is OpenGothic's `manaInvested`.
  (Distinct from `oCSpell+0x4c`, the 1-based spell *level* returned by `oCSpell::GetLevel`
  @ `0x00486620` — the already-fixed level path.)

Concrete trace for `aiExpectedInvest = E`: the original invests on the ticks where the
pre-tick `manaInvested` is `0,1,...,E` (release fires only when `manaInvested` reaches
`E+1`), so the NPC completes `E+1` invest ticks.

## OpenGothic file:line

`game/world/objects/npc.cpp:4146`, in `Npc::tickCast`:

```cpp
if(!isPlayer() && aiExpectedInvest<=manaInvested) {
  endCastSpell();
  return true;
  }
```

OpenGothic checks `manaInvested` (pre-increment, since `++manaInvested` runs later at
line 4163) against `aiExpectedInvest` with the same reference point as the original, but
uses **`<=`** instead of strict `<`. With `aiExpectedInvest = E` it releases once
`manaInvested >= E`, completing only `E` invest ticks.

## Divergence

OpenGothic stops one invest tick earlier than `Gothic2.exe`. For an NPC ordered to invest
to `aiExpectedInvest = E`, the original performs `E+1` invest ticks (manaInvested reaches
`E+1`) while OpenGothic performs `E` (manaInvested reaches `E`). Because the invest level
is driven up by the same tick loop, the NPC ends up casting its spell at one lower invest
level than the original game (weaker mage NPCs). `aiExpectedInvest` defaults to 1
(`npc.h:594`), so even the default-one-invest case is shifted.

## Proposed patch

OLD (`game/world/objects/npc.cpp:4146`):
```cpp
  if(!isPlayer() && aiExpectedInvest<=manaInvested) {
    endCastSpell();
    return true;
    }
```

NEW:
```cpp
  // NOTE: in original-game oCNpc::EV_CastSpell @0x0067fb20 the NPC keeps investing while
  // aiExpectedInvest >= manaInvested and only releases once manaInvested has *exceeded*
  // aiExpectedInvest (strict `<`, branch at `*(this+0x574) < *(oCSpell+0x48)`, with 0x574
  // = aiExpectedInvest set by oCNpc::ReadySpell @0x006802e0 and 0x48 = manaInvested from
  // oCSpell::Invest @0x004850d0). Using `<=` here releases one invest tick early, so NPC
  // spells were cast one invest level too low.
  if(!isPlayer() && aiExpectedInvest<manaInvested) {
    endCastSpell();
    return true;
    }
```

Grep-verified OG symbols: `aiExpectedInvest` (npc.cpp:2809 set from `act.i1`, npc.h:594
default 1, saved/loaded npc.cpp:224/286), `manaInvested` (npc.h:593), `isPlayer()`,
`endCastSpell()` (npc.cpp:4181). One-line, build-verifiable change.
