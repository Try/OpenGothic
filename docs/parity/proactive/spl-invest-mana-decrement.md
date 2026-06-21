# Spell invest: G2 engine never decrements caster mana

> DEFER: conflicts with an earlier spell sweep that rated the G2 mana-decrement Low (depends on unverified G1/G2 script Spell_ProcessMana behavior); adding a per-invest-tick mana charge materially changes spell mana cost. Needs runtime confirmation of net mana spent vs the script callback before applying.

**Confidence:** Medium

## Original function

`oCSpell::Invest` (Gothic2.exe @ 0x004850d0) is the per-tick invest driver in
Gothic 2. Each time the invest interval elapses it calls `Spell_ProcessMana`
(`CallScriptInvestedMana` @ 0x00485d30) which stores the returned spell-status
into oCSpell field +0x50. The engine then acts on that status:

- status **1 = SPL_RECEIVEINVEST**: when already investing it calls
  `oCNpc::ChangeAttribute(ATR_MANA, -1)` (FUN_0072ff60 @ 0x0072ff60, confirmed to
  add the delta to `attr[idx]` and clamp), then raises the invested-mana counter
  (+0x48). So the **engine itself spends 1 mana per RECEIVEINVEST tick.**
- status **8 = SPL_STATUS_CANINVEST_NO_MANADEC**: raises the invested counter but
  performs **no** `ChangeAttribute` call. The status name literally says
  "NO_MANADEC" — its sole purpose is to invest a level *without* the mana cost
  that RECEIVEINVEST normally incurs.

The existence of two distinct codes (1 vs 8) that differ only in whether mana is
spent proves the mana decrement on RECEIVEINVEST is the engine's responsibility,
not the script's.

## OpenGothic

`game/world/objects/npc.cpp:4015-4042` (`Npc::tickCast`). The engine-side mana
decrement is gated to Gothic 1 only:

```
4017  if(owner.version().game==1) {
4018    changeAttribute(ATR_MANA,-1,false);
```

The G2 invest switch (4028-4042) increments `manaInvested` for `SPL_NEXTLEVEL`,
`SPL_RECEIVEINVEST` and `SPL_STATUS_CANINVEST_NO_MANADEC` alike and **never**
decrements `ATR_MANA`. So in Gothic 2 OpenGothic never charges mana in the engine
during an invest cast, and it draws no distinction between RECEIVEINVEST (should
cost 1 mana) and CANINVEST_NO_MANADEC (should be free).

## Divergence

In vanilla G2, holding an invest spell drains 1 mana per RECEIVEINVEST tick via
the engine; `SPL_STATUS_CANINVEST_NO_MANADEC` is the exemption. In OpenGothic G2
no engine mana is spent on either, so invest spells cost less mana than vanilla
(unless the content scripts happen to subtract it themselves), and the
NO_MANADEC exemption is a no-op.

## Proposed patch

File: `game/world/objects/npc.cpp` (in `Npc::tickCast`, the G2 invest switch)

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
          castLevel = CastState(castLvl+1);
        visual.setMagicWeaponKey(owner,SpellFxKey::Invest,castLvl+1);
        }
      // NOTE: in original-game oCSpell::Invest (Gothic2.exe 0x004850d0) the engine
      // spends 1 mana per SPL_RECEIVEINVEST tick via oCNpc::ChangeAttribute(ATR_MANA,-1);
      // SPL_STATUS_CANINVEST_NO_MANADEC is the explicit no-mana-decrement exemption.
      if(owner.version().game==2 && code==SpellCode::SPL_RECEIVEINVEST)
        changeAttribute(ATR_MANA,-1,false);
      auto& spl = owner.script().spellDesc(active->spellId());
      castNextTime += uint64_t(spl.time_per_mana);
      ++manaInvested;
      return true;
      }
```
