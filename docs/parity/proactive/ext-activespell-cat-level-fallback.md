# External Npc_GetActiveSpellCat returns 0 (==SPELL_GOOD) on "no active spell"; original returns -1

Confidence: Medium (category). The level variant is noted but NOT claimed: in
the original GetActiveSpellLevel returns the *selected spell's* level whenever
in magic mode, while OG activeSpellLevel() returns *cast-progress* level (0
until channeling). That is a semantic difference, not a clean fallback bug, so
the level patch below is advisory only -- apply only after confirming intent.

## Original functions

- `oCNpc::GetActiveSpellCategory` 0x0073cfa0, called verbatim by external
  `Npc_GetActiveSpellCat` (handler 0x006e5960, DefineExternals_Ulfi).
- `oCNpc::GetActiveSpellLevel`    0x0073cfe0, called by external
  `Npc_GetActiveSpellLevel`.

Both return the spell's category / level ONLY when the NPC is in magic weapon
mode (weapon-mode field == 7) and the mag-book has a selected spell. In every
other case (not casting, no spellbook, no selected spell) both methods return
-1 (in prose: minus one). The external handlers forward the method result
unchanged to SetReturn.

## OpenGothic

game/game/gamescript.cpp:2848 `npc_getactivespellcat` returns `SPELL_GOOD`
(== 0) when npc is null / no active weapon / active weapon is not a spell.
game/game/gamescript.cpp:2869 `npc_getactivespelllevel` returns 0 in the same
fallback cases.

## Divergence

When there is no active spell the original returns -1, OG returns 0. For the
category this collides with the real value `SPELL_GOOD == 0`: a script branch
like `if(Npc_GetActiveSpellCat(self) == SPELL_GOOD)` matches in OG for an NPC
that is NOT casting any spell, whereas the original would return -1 and not
match. The level fallback (0 vs -1) is lower impact since 0 is not a valid
spell level the same way, but still differs from retail.

## Proposed patch

game/game/gamescript.cpp

OLD (npc_getactivespellcat):
```cpp
  auto npc = findNpc(npcRef);
  if(npc==nullptr)
    return SPELL_GOOD;

  const Item* w = npc->activeWeapon();
  if(w==nullptr || !w->isSpellOrRune())
    return SPELL_GOOD;
```

NEW:
```cpp
  // NOTE: in original-game oCNpc::GetActiveSpellCategory returns -1 when the
  // npc is not in magic mode / has no selected spell (NOT SPELL_GOOD==0).
  auto npc = findNpc(npcRef);
  if(npc==nullptr)
    return -1;

  const Item* w = npc->activeWeapon();
  if(w==nullptr || !w->isSpellOrRune())
    return -1;
```

OLD (npc_getactivespelllevel):
```cpp
  int  v   = 0;
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    v = npc->activeSpellLevel();
  return v;
```

NEW:
```cpp
  // NOTE: in original-game oCNpc::GetActiveSpellLevel returns -1 when there is
  // no active spell (not 0).
  auto npc = findNpc(npcRef);
  if(npc==nullptr)
    return -1;
  // activeSpellLevel() must itself yield -1 when no spell is active; if it
  // returns 0 in that case, guard on active weapon being a spell here.
  return npc->activeSpellLevel();
```

(Verify `activeSpellLevel()` already returns -1 for the no-spell case; if it
returns 0, gate it the same way as the category external before returning.)
