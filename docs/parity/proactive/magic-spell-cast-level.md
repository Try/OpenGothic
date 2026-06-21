# Magic: leveled spell scripts always cast at level 0

**Confidence:** High

## Original function

`oCMag_Book::Spell_Cast` (Gothic2.exe @ 0x004767a0). When the engine commits a
spell, it builds the script symbol name `Spell_Cast_<spellTag>` and, if that
symbol resolves, calls it passing **one integer argument: the spell level**
obtained from `oCSpell::GetLevel()` (@ 0x00486620, reads oCSpell field 0x4c).

That `level` field starts at 0 and is incremented once per "next level" tick
inside `oCSpell::Invest` (@ 0x004850d0, the `SPL_NEXTLEVEL` branch increments
field 0x4c). So:

- a non-invest / immediate-cast spell commits at level 0;
- an invest spell commits at level N, where N is the number of NEXTLEVEL ticks.

The leveled `Spell_Cast_<tag>(var int level)` script then scales its effect
(damage, radius, number of projectiles, ...) by this argument.

## OpenGothic

`GameScript::invokeSpell` — game/game/gamescript.cpp:1154-1155 hardcodes the
level it passes to the script:

```cpp
// FIXME: actually set the spell level!
int32_t  splLevel = 0;
```

`invokeSpell` is called from `Npc::commitSpell` (game/world/objects/npc.cpp:3193)
at a point where `castLevel` is in the `CS_Emit_*` range, so the correct
0-based level is `int(castLevel) - int(CS_Emit_0)` — exactly the value the shoot
path already uses one line apart (`lvl = (castLevel-CS_Emit_0)+1`, line 3196).

## Divergence

Every leveled spell script is invoked with `level == 0` regardless of how much
mana the caster actually invested. Invested mana raises projectile damage /
effect only through the C++ `damage_per_level*lvl` path; any scaling the script
itself performs from its `level` argument is lost, so invest spells behave as if
always cast at minimum level.

## Proposed patch

```
// game/game/gamescript.h
OLD: void invokeSpell(Npc& npc, Npc *target, Item&  fn);
NEW: void invokeSpell(Npc& npc, Npc *target, Item&  fn, int32_t splLevel);
```

```
// game/game/gamescript.cpp  (invokeSpell)
OLD:
void GameScript::invokeSpell(Npc &npc, Npc* target, Item &it) {
  auto&      tag = spellFxInstanceNames->get_string(uint16_t(it.spellId()));
  string_frm name("Spell_Cast_",tag);
  auto       fn = vm.find_symbol_by_name(name);
  if(fn==nullptr)
    return;

  // FIXME: actually set the spell level!
  int32_t  splLevel = 0;
  ScopeVar self (*vm.global_self(),  npc.handlePtr());
NEW:
void GameScript::invokeSpell(Npc &npc, Npc* target, Item &it, int32_t splLevel) {
  auto&      tag = spellFxInstanceNames->get_string(uint16_t(it.spellId()));
  string_frm name("Spell_Cast_",tag);
  auto       fn = vm.find_symbol_by_name(name);
  if(fn==nullptr)
    return;

  // NOTE: in original-game oCMag_Book::Spell_Cast passes oCSpell::GetLevel()
  // (the invested spell level, 0-based) as the script argument.
  ScopeVar self (*vm.global_self(),  npc.handlePtr());
```

```
// game/world/objects/npc.cpp  (Npc::commitSpell)
OLD:
  if(owner.version().game==2)
    owner.script().invokeSpell(*this,currentTarget,*active);
NEW:
  if(owner.version().game==2) {
    // NOTE: in original-game the script gets oCSpell::GetLevel() (0-based);
    // here castLevel is in the CS_Emit_* range, so subtract CS_Emit_0.
    const int32_t splLevel = int(castLevel) - int(CS_Emit_0);
    owner.script().invokeSpell(*this,currentTarget,*active,splLevel);
    }
```
