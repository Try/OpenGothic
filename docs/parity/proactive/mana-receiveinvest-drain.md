# G2 channeling/invest spells drain no mana (missing engine SPL_RECEIVEINVEST mana decrement)

**Confidence:** High

## Original function + address

`oCSpell::Invest` @ `0x004850d0` (Gothic2.exe) is the per-tick invest/channel loop
(driven for the player by `oCMag_Book` and for NPCs by `oCAIHuman::MagicInvestSpell`
@ `0x00472160`). Each time the accumulated frame-time crosses the spell's per-mana
threshold (`oCSpell+0x80`, default 2000.0f), it calls `oCSpell::CallScriptInvestedMana`
@ `0x00485d30`, which runs the Daedalus `Spell_ProcessMana(manaInvested)` and stores the
returned status code in `oCSpell+0x50`. The engine then reacts to that status:

- status `1` (`SPL_RECEIVEINVEST`): **if at least one mana was already invested**
  (`oCSpell+0x48 != 0`), the engine itself drains one point of the caster's mana via
  `oCNpc::ChangeAttribute(caster, attrIdx, -1)` (`@0x0072ff60`; caster is `oCSpell+0x34`,
  attribute index `oCSpell+0x7c` is initialized to `2` = `ATR_MANA` in
  `oCSpell::InitValues` @ `0x00484020`). Then `manaInvested++`.
- status `4` (`SPL_NEXTLEVEL`): `manaInvested++`, level++ — **no** mana drain.
- status `8` (`SPL_STATUS_CANINVEST_NO_MANADEC`): `manaInvested++` — **no** mana drain.
  Its name literally means "can invest, no mana-dec": it is the opt-out status a script
  uses when it deducts mana itself, which only makes sense because `SPL_RECEIVEINVEST` is
  the engine-deducts case.
- status with the `0x10000` bit (`SPL_FORCEINVEST`): set `manaInvested = amount` and
  drain `amount` mana at once.

So in original Gothic 2, channeling/invest spells whose `Spell_ProcessMana` returns
`SPL_RECEIVEINVEST` (e.g. Telekinesis / the light spells / control-type spells) have one
mana drained **by the engine** on every invest tick after the first.

## OpenGothic file:line

`game/world/objects/npc.cpp` — `Npc::tickCast`, the invest case block at lines
**4176-4189**. The only engine-side `ATR_MANA` decrement is at line **4160**, guarded by
`owner.version().game==1`. The Gothic-2 switch (lines 4176-4197) never decrements mana
for `SPL_RECEIVEINVEST`.

## Divergence

In Gothic 2, OpenGothic drains **no** mana for the `SPL_RECEIVEINVEST` channel path: the
`game==1` block at line 4160 is skipped, and the `SPL_RECEIVEINVEST` case only does
`castNextTime += time_per_mana; ++manaInvested;`. Mana for instant spells is still handled
correctly (the `Spell_Cast_<tag>` script deducts at commit, via `invokeSpell`), but
channeling/invest spells that return `SPL_RECEIVEINVEST` cost the caster nothing in
OpenGothic-G2, where the original engine drains 1 mana per tick. (Separately, OpenGothic
does not handle `SPL_FORCEINVEST` at all — it falls to the `default` "unexpected" log —
but that is a rarer path.)

The original's "skip the first tick" guard (`oCSpell+0x48 != 0`) is satisfied for free in
OpenGothic: the first invest is the `BeginCast` call (`manaInvested==0`, which performs no
drain), so by the time `tickCast` runs `manaInvested` is always `>= 1`.

## Proposed patch

`game/world/objects/npc.cpp`, inside the invest case (after the `SPL_NEXTLEVEL` level-up
block, before `castNextTime += ...`):

OLD:
```cpp
    case SpellCode::SPL_NEXTLEVEL:
    case SpellCode::SPL_RECEIVEINVEST:
    case SpellCode::SPL_STATUS_CANINVEST_NO_MANADEC: {
      if(code==SPL_NEXTLEVEL) {
        int32_t castLvl = int(castLevel)-int(CS_Invest_0);
        if(castLvl<15)
          castLevel = CastState(castLevel+1);
        visual.setMagicWeaponKey(owner,SpellFxKey::Invest,castLvl+1);
        }
      auto& spl = owner.script().spellDesc(active->spellId());
      castNextTime += uint64_t(spl.time_per_mana);
      ++manaInvested;
      return true;
      }
```

NEW:
```cpp
    case SpellCode::SPL_NEXTLEVEL:
    case SpellCode::SPL_RECEIVEINVEST:
    case SpellCode::SPL_STATUS_CANINVEST_NO_MANADEC: {
      if(code==SPL_NEXTLEVEL) {
        int32_t castLvl = int(castLevel)-int(CS_Invest_0);
        if(castLvl<15)
          castLevel = CastState(castLevel+1);
        visual.setMagicWeaponKey(owner,SpellFxKey::Invest,castLvl+1);
        }
      // NOTE: in original-game oCSpell::Invest @0x004850d0, when Spell_ProcessMana returns
      // SPL_RECEIVEINVEST and at least one mana was already invested (oCSpell+0x48 != 0), the
      // engine drains one mana from the caster: oCNpc::ChangeAttribute(caster,ATR_MANA,-1)
      // @0x0072ff60 (attribute index oCSpell+0x7c defaults to ATR_MANA in InitValues @0x00484020).
      // SPL_NEXTLEVEL and SPL_STATUS_CANINVEST_NO_MANADEC do NOT drain (the latter's name means
      // "no mana-dec"). G1 already drains above (line 4160); the G2 RECEIVEINVEST path was missing,
      // so G2 channeling/invest spells cost no mana. The first invest is BeginCast (manaInvested==0,
      // no drain), so in tickCast manaInvested is always >=1, matching the original "!= 0" guard.
      if(owner.version().game==2 && code==SPL_RECEIVEINVEST && manaInvested>0)
        changeAttribute(ATR_MANA,-1,false);
      auto& spl = owner.script().spellDesc(active->spellId());
      castNextTime += uint64_t(spl.time_per_mana);
      ++manaInvested;
      return true;
      }
```

Grep-verified symbols: `Npc::changeAttribute(Attribute,int32_t,bool)` (npc.cpp:1244),
`ATR_MANA` (constants.h:475), `SPL_RECEIVEINVEST` (constants.h:486),
`owner.version().game==2` (npc.cpp:480 et al.), `manaInvested` field (npc.cpp).

Caveat to validate at runtime: the fix assumes G2 base-game `Spell_ProcessMana` does not
itself deduct mana on the `SPL_RECEIVEINVEST` path (it returns
`SPL_STATUS_CANINVEST_NO_MANADEC` when it does). The distinct existence of the
"NO_MANADEC" status, both codes being engine-handled, makes this the intended split.
