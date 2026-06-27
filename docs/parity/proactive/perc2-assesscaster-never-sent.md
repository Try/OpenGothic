# PERC_ASSESSCASTER is never dispatched when an NPC/player begins channeling a spell

**Confidence:** High (that the perception is dispatched by the original and absent in OpenGothic). Medium on the exact re-broadcast frequency (see NOTE).

## Original function + address (prose)

In `Gothic2.exe`, `oCAIHuman::MagicInvestSpell` (entry `0x00472160`) drives the
spell-investing/channeling stage that both NPCs and the player run while holding the
magic action. Each invest cycle, after it calls `oCMag_Book::Spell_Invest` and the
spell has actually received an invest (the spell's invested-mana field at `+0x48` is
non-zero) and its status is still non-zero, it performs the invest animation
transition (`oCAniCtrl_Human::TransitionToInvest`) and then calls
`oCNpc::AssessCaster_S` (entry `0x0075d200`).

`oCNpc::AssessCaster_S` is a one-liner: it calls
`oCNpc::CreatePassivePerception` (entry `0x0075b270`) with perception type
`0x1d` (= 29 = `PERC_ASSESSCASTER`), `OTHER` = the caster itself (`this`) and
`VICTIM` = null. `CreatePassivePerception` collects every NPC within the
`PERC_ASSESSCASTER` perception range, and for each receiver that has perception
`0x1d` active (and is not the sender, not dead, not the player), sets the script
`OTHER` instance to the caster and starts that receiver's `PERC_ASSESSCASTER`
AI-state. In other words: "an NPC near me started casting a spell", with `OTHER`
pointing at the caster. This is distinct from `PERC_ASSESSMAGIC`, which is sent to
the spell's *target* on impact (already handled by OpenGothic).

The same `MagicInvestSpell` path runs for the player (the player is an
`oCAIHuman`), so the original broadcasts `PERC_ASSESSCASTER` for player casting too,
which is how bystander NPCs can react to the player charging a spell.

## OpenGothic file:line

- `game/world/objects/npc.cpp:4177-4186` — `Npc::beginCastSpell()`, the
  `SPL_STATUS_CANINVEST_NO_MANADEC / SPL_RECEIVEINVEST / SPL_NEXTLEVEL` branch that
  enters the invest stage (`castLevel = CS_Invest_0; return BC_Invest;`).
- `beginCastSpell()` is the single invest-entry point used by **both** casting paths:
  NPC fight AI at `game/world/objects/npc.cpp:1748` and player control at
  `game/game/playercontrol.cpp:816`.

## Divergence

OpenGothic never dispatches `PERC_ASSESSCASTER` anywhere. A grep of the entire
`game/` tree finds the constant only at its definition
(`game/game/constants.h:438`); there is no `sendPassivePerc(... PERC_ASSESSCASTER)`
and no other send site. Consequently, NPCs that have enabled `PERC_ASSESSCASTER`
(via `Npc_PercEnable`) never receive the "someone near me is casting a spell"
notification, whereas the original broadcasts it to all in-range NPCs whenever a
caster (NPC or player) enters/continues the invest stage. Receivers that did not
enable the perception are unaffected either way (the receiver-side gate
`Npc::perceptionProcess` → `hasPerc` already filters them out), so adding the send
cannot spam NPCs that do not care.

## Proposed patch

The wiring matches the existing OpenGothic passive-perception broadcasts: self =
caster, other = caster, no victim — i.e. `World::sendPassivePerc(Npc&,Npc&,int32_t)`
(`game/world/world.cpp:706` → `wobj.sendPassivePerc(self,other,nullptr,nullptr,perc)`),
which is exactly the `CreatePassivePerception(this,0x1d,OTHER=this,VICTIM=null)`
shape. Insert the broadcast at the invest-entry transition in `beginCastSpell()`,
covering both the NPC AI and player-control casters through the one shared call.

OLD (`game/world/objects/npc.cpp`, `Npc::beginCastSpell()` invest branch):
```cpp
    case SPL_STATUS_CANINVEST_NO_MANADEC:
    case SPL_RECEIVEINVEST:
    case SPL_NEXTLEVEL: {
      ++manaInvested;
      auto ani = owner.script().spellCastAnim(*this,*active);
      if(!visual.startAnimSpell(*this,ani,true))
        Log::d("Couldn't start animation for spell '",currentSpellCast,"'");
      castLevel = CS_Invest_0;
      return BeginCastResult::BC_Invest;
      }
```

NEW:
```cpp
    case SPL_STATUS_CANINVEST_NO_MANADEC:
    case SPL_RECEIVEINVEST:
    case SPL_NEXTLEVEL: {
      ++manaInvested;
      auto ani = owner.script().spellCastAnim(*this,*active);
      if(!visual.startAnimSpell(*this,ani,true))
        Log::d("Couldn't start animation for spell '",currentSpellCast,"'");
      castLevel = CS_Invest_0;
      // NOTE: in original-game oCAIHuman::MagicInvestSpell @0x00472160 -> oCNpc::AssessCaster_S
      // @0x0075d200 -> oCNpc::CreatePassivePerception @0x0075b270 broadcasts PERC_ASSESSCASTER
      // (0x1d) to nearby NPCs whenever a caster (NPC or player) is investing a spell, with
      // OTHER=the caster and no VICTIM. OpenGothic never sent PERC_ASSESSCASTER, so bystanders
      // could not react to a spell being cast near them. Original re-sends on each invest cycle;
      // this surgical fix broadcasts once at invest-start (the TransitionToInvest moment).
      owner.sendPassivePerc(*this,*this,PERC_ASSESSCASTER);
      return BeginCastResult::BC_Invest;
      }
```

### Notes / residual risk
- Symbols grep-verified to exist: `PERC_ASSESSCASTER` (`game/game/constants.h:438`);
  `World::sendPassivePerc(Npc&,Npc&,int32_t)` (`game/world/world.h:170`,
  `game/world/world.cpp:706`); the receiver-side generic dispatch + active-gate in
  `Npc::perceptionProcess` / `hasPerc` (`game/world/objects/npc.cpp:4537-4565`).
- Frequency caveat (the Medium part of the confidence): the original re-broadcasts on
  every invest cycle (each `MagicInvestSpell` tick while channeling), not only at the
  first invest. This patch fires once when channeling starts, which fixes the
  qualitative divergence (never sent at all) without per-tick perception spam. If
  exact frequency parity is desired, an equivalent `owner.sendPassivePerc(*this,*this,
  PERC_ASSESSCASTER)` could additionally be placed in the per-cycle invest branch of
  `Npc::tickCast` (`game/world/objects/npc.cpp:4279-4301`); left out here to keep the
  fix surgical.
