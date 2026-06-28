# Scroll consume / cast / removal — NO FINDING

**Confidence:** NO FINDING (scroll consume/removal path is a faithful reimplementation; the
only behavioral divergence found lies in the explicitly-excluded mana-invest territory).

## Scope

Audited the one-shot spell-scroll cast → consume → removal/auto-switch path:
- OpenGothic: `Npc::commitSpell()` — `game/world/objects/npc.cpp:3520-3538` (the `active->isSpell()`
  consume block), `Npc::tickCast`/`beginCastSpell` state machine (npc.cpp:4270-4475),
  `Item::isSpell`/`isMulti`/`isSpellOrRune` (`game/world/objects/item.cpp:251-284`),
  `Inventory::currentSpell` (`game/game/inventory.h:110`).
- Original `Gothic2.exe`: `oCMag_Book::Spell_Cast` @0x004767a0 (decrement field 0x32c, remove on
  zero, switch to slot 0 / `oCMsgWeapon(3)` holster), `oCItem::MultiSlot` @0x007125a0,
  `oCSpell::Cast` @0x00485360, `oCSpell::Invest` @0x004850d0,
  `oCAIHuman::MagicCheckSpellStates` @0x00472770 (the only caller of Spell_Cast).

## What was verified faithful

- **Scroll-vs-rune predicate.** OG `Item::isSpell()` = `isSpellOrRune() && isMulti()` (ITM_MULTI,
  bit 21). Original `oCItem::MultiSlot` @0x007125a0 reads the same flag bit (0x200000) plus generic
  stackable-category bits; within the magic-book context (item already a spell/rune) the ITM_MULTI
  bit cleanly separates the consumable scroll from the reusable rune. Equivalent.
- **Decrement on cast.** OG `cnt = active->count(); invent.delItem(active->clsId(),1,*this)` matches
  the original `count(0x32c) -= 1`; stacked scrolls (count > 1) keep the item selected, exactly like
  the original's `if(count==0)`-gated removal.
- **Last-charge removal + auto-switch.** OG, on `cnt<=1`, picks the first remaining equipped spell
  (`currentSpell(0..7)`) and `drawSpell`s it, else `aiRemoveWeapon()`. The original does
  `RemoveFromInv` → reset book index to 0 + re-`Setup` slot-0 spell, else send `oCMsgWeapon(3)`
  (holster) when the book is empty. Functionally equivalent (switch to first remaining spell, or
  put the book away).

## The only divergence found — and why it is excluded

Original `oCAIHuman::MagicCheckSpellStates` @0x00472770 only calls `oCMag_Book::Spell_Cast` (the
function that consumes the scroll) when `GetSpellStatus()==2` **and** `oCSpell` field 0x48
(investedMana) `!= 0`. `oCSpell::Cast` @0x00485360 likewise returns "cast done" (1) only when field
0x48 `> 0`; with 0 invested mana it returns 0 and the scroll is neither fired nor decremented. In
other words the original gates the scroll's *fire + consume* on at least one invested-mana tick,
whereas OpenGothic's `commitSpell()` consumes unconditionally once the emit stage is reached for a
SPL_SENDCAST scroll (manaInvested can be 0).

This sits squarely inside the **already-handled / excluded** "scroll bypasses mana-invest, casts at
SPL_SENDCAST even at 0 mana" decision that OpenGothic documents deliberately at npc.cpp:4280-4289.
Re-litigating it would contradict that recorded design choice and is out of scope per the task's
exclusion list. No fresh, surgical, build-verifiable divergence exists outside that area.

**Result: NO FINDING.**
