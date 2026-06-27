# Scroll cast wrongly blocked at zero mana (engine-side mana pre-gate ignores scrolls)

**Confidence:** Medium

## Original function + address

`oCNpc::EV_CastSpell` (Gothic2.exe `0x0067fb20`) is the engine magic-frame handler. When
the cast event fires it fetches the selected spell from the spell book
(`oCMag_Book::GetSelectedSpell`) and unconditionally drives the cast through
`oCAIHuman::MagicInvestSpell` / `oCMag_Book::Spell_Cast`. There is **no engine-side
`mana <= 0` precondition** that aborts the cast before the script runs: the only mana
comparison in that path is `npc_mana (oCNpc+0x574) < spell_invest_cost (oCSpell+0x48)`,
which merely selects the invest vs. cast branch, never a hard refusal. The actual
"not enough mana" decision is delegated to the spell script (`Spell_Processing` /
`Spell_ProcessMana`), which returns the cast status.

`oCMag_Book::Spell_Cast` (Gothic2.exe `0x004767a0`) then consumes the item only on the
MultiSlot/scroll path (`oCItem::MultiSlot` of the equipped spell item, stack-count field
`oCItem+0x32c`), with no mana requirement gating the consumption. Scrolls (the MultiSlot /
`ITM_MULTI` rune-category items) resolve to the immediate `SPL_SENDCAST` status and cast
without ever entering the mana-investment loop (`oCSpell::Invest` `0x004850d0`, which is the
only place the npc mana attribute is decremented during a cast). In short: in the original a
scroll casts and is consumed even when the caster has 0 mana, because scrolls bypass the
mana-investment ("mana circle") stage entirely and the engine never pre-checks mana.

## OpenGothic file:line

`game/world/objects/npc.cpp:4028` (inside `Npc::beginCastSpell`).

## Divergence

`Npc::beginCastSpell` adds an engine-side hard gate that the original does not have:

```cpp
4023  auto active=invent.activeWeapon();
4024  if(active==nullptr)
4025    return BeginCastResult::BC_No;
4026
4027  setAnimRotate(0);
4028  if(attribute(ATR_MANA)<=0) {
4029    setAnim(Anim::MagNoMana);
4030    return BeginCastResult::BC_NoMana;
4031    }
```

This fires for every spell/rune/scroll. But just below (line 4039) OpenGothic already calls
`owner.script().invokeMana(...)` — the faithful port of the script-side mana arbiter — which
returns `SPL_DONTINVEST`/`SPL_SENDSTOP` for a rune with no mana (→ `MagNoMana`, line 4042)
and `SPL_SENDCAST` for a scroll (→ immediate `CS_Cast_0`, line 4058). So the script path
already handles the no-mana case correctly for both item kinds. The redundant pre-gate at
line 4028 short-circuits before that script call and wrongly aborts a **scroll** cast when
the caster sits at exactly 0 mana — defeating the defining scroll mechanic (cast a spell you
have no mana/circle for, one-shot). Runes are unaffected (the pre-gate result matches the
script result for them).

`Item::isSpell()` (`game/world/objects/item.cpp:276`) is the existing scroll predicate
(`isSpellOrRune() && isMulti()`), i.e. the MultiSlot/`ITM_MULTI` flag the original keys on.

## Proposed patch

OLD (`game/world/objects/npc.cpp:4028`):
```cpp
  setAnimRotate(0);
  if(attribute(ATR_MANA)<=0) {
    setAnim(Anim::MagNoMana);
    return BeginCastResult::BC_NoMana;
    }
```

NEW:
```cpp
  setAnimRotate(0);
  // NOTE: in original-game oCNpc::EV_CastSpell @0x0067fb20 has no engine-side mana<=0
  // pre-gate; the no-mana decision is left to the spell script. Scrolls (oCItem::MultiSlot,
  // oCMag_Book::Spell_Cast @0x004767a0) bypass the mana-invest stage and cast via SPL_SENDCAST
  // even at 0 mana. invokeMana() below already returns the correct status for runes, so only
  // skip the redundant pre-gate for scrolls to avoid wrongly refusing a 0-mana scroll cast.
  if(attribute(ATR_MANA)<=0 && !active->isSpell()) {
    setAnim(Anim::MagNoMana);
    return BeginCastResult::BC_NoMana;
    }
```

Grep-verified symbols: `Item::isSpell() const` (`game/world/objects/item.h:71`), `active`
is `invent.activeWeapon()` returning `Item*` and already null-checked above; `ATR_MANA`,
`Anim::MagNoMana`, `BeginCastResult::BC_NoMana` all used verbatim at the original site.

Residual uncertainty (hence Medium): the original's no-mana refusal is script-driven, so the
exact threshold behavior for a 0-mana scroll relies on the stock `Spell_Processing` returning
`SPL_SENDCAST`; the engine evidence (no pre-gate + MultiSlot consume with no mana condition +
scrolls skipping `oCSpell::Invest`) strongly supports it but was not cross-checked against the
Daedalus scripts.
