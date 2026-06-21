# Magic: leveled Spell_Cast_<tag> receives an off-by-one (0-based) level

**Confidence:** High

## Original function + address

`oCMag_Book::Spell_Cast` (Gothic2.exe @ `0x004767a0`) commits a spell. It builds
the script symbol name `SPELL_CAST_<spellTag>` and, when that symbol resolves,
calls it passing **one integer argument**: the value returned by
`oCSpell::GetLevel()`.

`oCSpell::GetLevel` (@ `0x00486620`) is a trivial accessor that returns the
`oCSpell` field at offset `0x4c` unchanged.

That `0x4c` level field is **initialized to 1**, not 0:
`oCSpell::InitByScript` (@ `0x00484550`) writes `field[0x4c] = 1` (alongside
`field[0x48] = 0`, the separate mana-invested counter). The same init path runs
for the freshly-created `oCSpell` that `Spell_Cast` instantiates for the cast.

The level is then incremented by 1 per "next level" tick in `oCSpell::Invest`
(@ `0x0048525e`): only the `SPL_NEXTLEVEL` (code `4`) branch does
`field[0x4c] = field[0x4c] + 1`. No path decrements or zero-bases it. So the
argument the engine hands to `SPELL_CAST_<tag>(var int level)` is:

- `1` for a non-invest / immediate cast (zero NEXTLEVEL ticks);
- `1 + N` for an invest spell with `N` NEXTLEVEL ticks.

i.e. the script level is **1-based**.

Note that the related `SPELL_PROCESSMANA(var int manaInvested)` callback is a
*different* argument: `oCSpell::CallScriptInvestedMana` passes field `0x48`
(mana-invested counter, init 0) — so that one is correctly 0-based. The two must
not be conflated; `Spell_Cast` specifically uses `GetLevel()` / `0x4c`.

## OpenGothic file:line

`game/world/objects/npc.cpp:3210-3215` (`Npc::commitSpell`) and
`game/game/gamescript.cpp:1147-1170` (`GameScript::invokeSpell`).

`commitSpell` computes the level it forwards to the script as:

```cpp
const int32_t splLevel = int(castLevel) - int(CS_Emit_0);  // 0-based
owner.script().invokeSpell(*this,currentTarget,*active,splLevel);
```

By the time `commitSpell` runs, `castLevel` is in the `CS_Emit_*` range and
`castLevel - CS_Emit_0` equals `N`, the number of NEXTLEVEL increments (0 for an
immediate cast). This is **0-based**, one less than `oCSpell::GetLevel()`.

The accompanying source comment (npc.cpp:3211) asserts the original passes a
"0-based invested level", which is incorrect — `GetLevel()` is 1-based
(InitByScript sets it to 1).

## Divergence

Every leveled spell script `Spell_Cast_<tag>` is invoked with a level one lower
than the original engine: an immediate cast passes `0` where the original passes
`1`; an invest spell with `N` ticks passes `N` where the original passes `N+1`.

Spell scripts that scale by their `level` argument (damage, radius, projectile
count, status duration, ...) therefore behave as a strictly weaker tier than in
the original. A script written `if(level==1) {weak} else if(level==2) {strong}`
fires its level-1 branch where the original would fire level-2, etc. Scripts that
index a per-level table by `level` would read the wrong slot (or slot `-1`).

This is internally inconsistent within OpenGothic itself: the projectile path one
line below uses `(castLevel-CS_Emit_0)+1` (npc.cpp:3218) for damage scaling, and
`Npc::activeSpellLevel()` (npc.cpp:4090-4095, the source of the
`Npc_GetActiveSpellLevel` external) returns `... +1` — both already expose the
**1-based** level to game logic/scripts. Only `invokeSpell` is off by one.

## Proposed patch

```cpp
// game/world/objects/npc.cpp  (Npc::commitSpell)
OLD:
  if(owner.version().game==2) {
    // NOTE: the original passes oCSpell::GetLevel() (0-based invested level); here castLevel
    // is in the CS_Emit_* range, so the 0-based level is castLevel-CS_Emit_0.
    const int32_t splLevel = int(castLevel) - int(CS_Emit_0);
    owner.script().invokeSpell(*this,currentTarget,*active,splLevel);
    }
NEW:
  if(owner.version().game==2) {
    // NOTE: in original-game oCMag_Book::Spell_Cast @0x004767a0 passes oCSpell::GetLevel()
    // (@0x00486620, field 0x4c). That field is initialized to 1 by oCSpell::InitByScript
    // @0x00484550 and incremented per SPL_NEXTLEVEL in oCSpell::Invest @0x0048525e, so the
    // script level is 1-based. castLevel is in the CS_Emit_* range here, hence +1.
    const int32_t splLevel = int(castLevel) - int(CS_Emit_0) + 1;
    owner.script().invokeSpell(*this,currentTarget,*active,splLevel);
    }
```

This forwards `splLevel+1`, matching `oCSpell::GetLevel()` and aligning with the
already-1-based shoot-damage path (npc.cpp:3218) and `activeSpellLevel()`
(npc.cpp:4092/4094). No signature change is required — `invokeSpell` already
takes `int32_t splLevel` (`game/game/gamescript.cpp:1147`).
