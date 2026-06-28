# Invest/channel spells charge one mana-level too fast (missing first `time_per_mana` gap)

**Confidence:** Medium (mechanism is unambiguous from the decompile; the observable magnitude is bounded — a one-level / one-`time_per_mana` shift).

## Original function + address

`oCSpell::Invest` @ `0x004850d0` drives one channeling tick. The per-mana cadence is an
accumulator: `oCSpell+0x44` is a running timer, `oCSpell+0x80` is the spell's `time_per_mana`,
and `oCSpell+0x48` is `manaInvested`. Each tick it does:

- `inc = DAT_0099b3d8` (the global per-frame time delta), **except** on the very first invest
  (`manaInvested == 0`) where `inc = time_per_mana` — forcing the first invest to fire immediately
  and also broadcasting `PERC_ASSESSCASTER` once;
- `accumulator += inc`;
- if `accumulator >= time_per_mana`: `accumulator -= time_per_mana`, then call the script
  (`CallScriptInvestedMana` @ `0x00485d30` → `Spell_ProcessMana`), drain mana on `SPL_RECEIVEINVEST`,
  `++manaInvested`, and on `SPL_NEXTLEVEL` bump the spell level (`oCSpell+0x4c`) + `InvestNext` the FX.

Net cadence: invest #1 at `t0` (immediate), invest #2 at `t0 + time_per_mana`, invest #3 at
`t0 + 2*time_per_mana`, … The first call to `Spell_ProcessMana` only ever happens inside the first
`Invest` (`oCMag_Book::Spell_Setup`/`oCSpell::Setup` @ `0x00484930` only sets the status field
`+0x50`; it never calls the script nor touches `manaInvested`), so the immediate first invest *is*
that first script call.

## OG file:line

`/Users/admin/Downloads/opengothic/game/world/objects/npc.cpp:4182` (`beginCastSpell`, the
`castNextTime = owner.tickCount();` line) and the invest case at lines 4195–4211; the consumer is
the invest-loop gate in `tickCast` at line 4284 (`if(owner.tickCount()<castNextTime) return true;`).

## Divergence

`Npc::beginCastSpell()` performs the immediate first invest itself (calls `invokeMana`, sets
`manaInvested = 1`, `castLevel = CS_Invest_0`) — this correctly mirrors the original's immediate
invest #1. But it leaves `castNextTime` at `owner.tickCount()` (set at line 4182) and never advances
it. Consequently the very next `tickCast()` (once `BS_CASTING` is reached, typically the next frame)
already passes the `owner.tickCount() < castNextTime` gate and performs invest #2 **immediately**,
only then doing `castNextTime += time_per_mana`. The original instead waits a full `time_per_mana`
before invest #2.

Effect: every invest/channel spell (Firestorm, Firerain, Ice Wave, summon/charged spells, etc.) and
every NPC invest cast (`aiExpectedInvest`) is shifted one whole `time_per_mana` interval earlier —
the spell reaches each invested mana-level (and drains the corresponding mana) one level/one
`time_per_mana` ahead of `Gothic2.exe`, and an NPC releases its cast `time_per_mana` sooner.

## Proposed patch

In `Npc::beginCastSpell()`, the invest branch (the
`SPL_STATUS_CANINVEST_NO_MANADEC / SPL_RECEIVEINVEST / SPL_NEXTLEVEL` case):

OLD:
```cpp
    case SPL_STATUS_CANINVEST_NO_MANADEC:
    case SPL_RECEIVEINVEST:
    case SPL_NEXTLEVEL: {
      ++manaInvested;
      auto ani = owner.script().spellCastAnim(*this,*active);
```

NEW:
```cpp
    case SPL_STATUS_CANINVEST_NO_MANADEC:
    case SPL_RECEIVEINVEST:
    case SPL_NEXTLEVEL: {
      ++manaInvested;
      // NOTE: in original-game oCSpell::Invest @0x004850d0 the per-mana invest cadence is a
      // frame-time accumulator (oCSpell+0x44) gated by time_per_mana (oCSpell+0x80): ONLY the first
      // invest (manaInvested==0) is forced immediate, every later invest waits a full time_per_mana.
      // This BeginCast IS that immediate first invest, so the next invest must be delayed by
      // time_per_mana. Leaving castNextTime at tickCount() let the very next tickCast() perform the
      // second invest ~one frame later instead of after time_per_mana, charging invest/channel spells
      // (and NPC aiExpectedInvest releases) one mana-level too fast.
      castNextTime = owner.tickCount() + uint64_t(owner.script().spellDesc(active->spellId()).time_per_mana);
      auto ani = owner.script().spellCastAnim(*this,*active);
```

(`active` is non-null here; `active->spellId()`, `owner.script().spellDesc(...)`,
`zenkit::ISpell::time_per_mana`, and `owner.tickCount()` are all already used in this file —
lines 4245/4324/4325.)
