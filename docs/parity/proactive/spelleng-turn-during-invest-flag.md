# Spell casting ignores `canTurnDuringInvest`: NPCs always rotate to target while channeling

**Confidence:** Medium-High

## Original function + address (prose only)

The per-frame turn-toward-enemy that happens while an NPC channels/invests a spell
lives in `oCAIHuman::MagicMode` at **0x00472fd0**. Inside its magic-mode tick, before
it calls `MagicInvestSpell`, it reads the currently-selected `oCSpell` and **gates the
turn on a single spell field**: it calls `oCNpc::TurnToEnemy(caster)` *only* when
`oCSpell` field `0x90` is non-zero. When that field is zero the engine skips the
turn entirely and the caster keeps its current heading for the whole cast.

Field `0x90` is the spell descriptor member `canTurnDuringInvest`. This is confirmed by
walking the `oCSpell` layout from the already-verified anchor `0x9c =
target_collect_algo` (used as the caster-only test in `oCSpell::IsValidTarget`
@0x004861d0, and as the collect-range/azi/elev block at 0xa4/0xa8/0xac in
`oCSpell::IsValidTarget`). The C_Spell field order is
`time_per_mana, damage_per_level, damage_type, spell_type, canTurnDuringInvest,
canChangeTargetDuringInvest, isMultiEffect, target_collect_algo, ...`, which maps to
`oCSpell` offsets `0x80, 0x84, 0x88, 0x8c, 0x90, 0x94, 0x98, 0x9c, ...`. So `0x90 =
canTurnDuringInvest` and `0x94 = canChangeTargetDuringInvest` (the latter is the field
`oCSpell::Setup` @0x00484930 reads at `0x94` to decide whether a re-Setup with a new
target is allowed). `0x80 = time_per_mana` is independently confirmed (used as the
invest accumulator step in `oCSpell::Invest` @0x004850d0).

Net original behavior: an NPC turns to face its target during spell channeling **iff
the spell's `canTurnDuringInvest` flag is set**. Spells authored with
`canTurnDuringInvest == 0` (self-buffs / non-aimed control spells) are cast without the
caster rotating.

## OpenGothic file:line

`game/world/objects/npc.cpp:4167-4169` (`Npc::tickCast`):

```cpp
if(!isPlayer() && currentTarget!=nullptr) {
  implTurnTo(*currentTarget,AnimationSolver::TurnType::None,dt);
  }
```

This turns the casting NPC toward `currentTarget` on **every** cast tick,
unconditionally. The spell flag `can_turn_during_invest` is exposed on the descriptor
(`zenkit::ISpell::can_turn_during_invest`, `lib/ZenKit/include/zenkit/addon/daedalus.hh:383`)
but is **never read anywhere in `game/`** (grep-verified: only `spell_type` and
`target_collect_*` of `ISpell` are consumed; `can_turn_during_invest` and
`can_change_target_during_invest` have zero usages).

## Divergence

For a spell whose `canTurnDuringInvest == 0`, the original keeps the caster's heading
fixed for the entire channel, while OpenGothic still rotates the NPC to face its
current target every tick. Visible effect: an NPC casting such a spell pivots toward
its enemy in OpenGothic where vanilla would not, and the conditional in
`oCAIHuman::MagicMode` proves vanilla content relies on the flag being honored
(an always-on turn would make the `field 0x90 != 0` guard dead code).

## Proposed patch

`active` is already non-null and `isSpellOrRune()`-checked at this point in
`tickCast` (lines 4156-4165), so `active->spellId()` is safe.

OLD (`game/world/objects/npc.cpp:4167`):
```cpp
    if(!isPlayer() && currentTarget!=nullptr) {
      implTurnTo(*currentTarget,AnimationSolver::TurnType::None,dt);
      }
```

NEW:
```cpp
    if(!isPlayer() && currentTarget!=nullptr) {
      // NOTE: in original-game oCAIHuman::MagicMode @0x00472fd0 the caster turns toward its
      // target during channeling only when the spell's canTurnDuringInvest flag is set
      // (oCSpell field 0x90; it gates the TurnToEnemy() call). Spells authored with
      // canTurnDuringInvest==0 are cast without rotating. OpenGothic turned unconditionally.
      const auto& spl = owner.script().spellDesc(active->spellId());
      if(spl.can_turn_during_invest!=0)
        implTurnTo(*currentTarget,AnimationSolver::TurnType::None,dt);
      }
```

Grep-verified symbols: `GameScript::spellDesc(int32_t) -> const zenkit::ISpell&`
(`game/game/gamescript.h:117`); `zenkit::ISpell::can_turn_during_invest`
(`lib/ZenKit/include/zenkit/addon/daedalus.hh:383`); `Item::spellId()` and
`Npc::owner.script()` already used in this file (e.g. `npc.cpp:3318-3319`).
