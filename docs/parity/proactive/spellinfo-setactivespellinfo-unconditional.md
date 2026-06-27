# Npc_SetActiveSpellInfo stores the transform-instance unconditionally; original only stores it while a spell is actively selected (magic mode)

Confidence: Medium. The gate omission is grep-verified and the original
behavior is unambiguous; the impact on retail scripts is small (vanilla calls
this only mid-cast, where both branches agree), but the OG version can latch a
stale `spellInfo` that later forces an unintended transform, so the fix matches
the original and is low-risk.

## Original function + address

- `oCNpc::SetActiveSpellInfo(int)` at Gothic2.exe 0x0073cf20, called verbatim by
  the script external `Npc_SetActiveSpellInfo`.

The original method first clamps the weapon-mode field (oCNpc offset 0x250) into
the range 0..7, then does work ONLY when the npc is in magic weapon mode
(field == 7) AND `oCMag_Book::GetSelectedSpell` returns a non-null spell. In
that case it writes the info onto the *selected spell object* via
`oCSpell::SetSpellInfo` (0x00486940, oCSpell field 0x58). In every other case
(not in magic mode, no spellbook, nothing selected) it is a no-op -- the value
is dropped.

That per-spell field 0x58 is consumed later by `oCSpell::CastSpecificSpell`
(0x00486960): for transform-category spells it is read back as the npc-instance
index to transform the caster into. Because the info lives on the transient
oCSpell object (recreated each time a rune/spell is drawn) and is only ever set
while that spell is the active/selected one, it cannot leak into an unrelated
later cast.

## OpenGothic file:line

- `game/world/objects/npc.cpp:4208` `Npc::setActiveSpellInfo`
- `game/world/objects/npc.cpp:3356` (consumer in `commitSpell`: transforms when
  `spellInfo != 0`)
- `game/game/gamescript.cpp:2971` `npc_setactivespellinfo` (external handler)

## Divergence

OpenGothic stores the value unconditionally on the *npc* (`spellInfo = info;`)
with no magic-mode / selected-spell gate, and `commitSpell` later transforms the
caster whenever `spellInfo != 0`. `spellInfo` is only cleared at the end of a
cast (npc.cpp:4133) or right after a transform commit (npc.cpp:3362) -- it is
NOT cleared when a new cast starts.

Consequence: if `Npc_SetActiveSpellInfo(self, X)` is ever invoked when the npc
is not actively channeling a spell (the original treats this as a no-op), OG
latches `X` onto the npc. The next spell the npc casts then sees
`spellInfo != 0` in `commitSpell` and transforms the caster into instance `X`,
an effect the original would never produce. Retail G2 scripts only call this
external from inside the `Spell_Cast_*` transform handlers (mid-cast, magic
weapon drawn), where both implementations agree, so vanilla is unaffected; the
divergence bites modified/edge-case call sites.

## Proposed patch

Gate the store exactly as the original does. During a legitimate transform cast
the rune is the active weapon (`currentSpellCast` is derived from
`activeWeapon()`, npc.cpp:4036/4053), so `activeWeapon()->isSpellOrRune()` is
true and the info is still stored -- vanilla transforms keep working -- while
out-of-cast calls become the no-op the original intends.

`game/world/objects/npc.cpp`

OLD:
```cpp
void Npc::setActiveSpellInfo(int32_t info) {
  spellInfo = info;
  }
```

NEW:
```cpp
void Npc::setActiveSpellInfo(int32_t info) {
  // NOTE: in original-game oCNpc::SetActiveSpellInfo @0x0073cf20 stores the
  // spell info only while the npc is in magic weapon mode with a selected
  // spell (it writes oCSpell::SetSpellInfo on the active spell); otherwise it
  // is a no-op. Gate on the active spell/rune so a stale value cannot later
  // force an unintended transform in commitSpell.
  Item* w = activeWeapon();
  if(w==nullptr || !w->isSpellOrRune())
    return;
  spellInfo = info;
  }
```

Symbols verified to exist: `Npc::activeWeapon()` (npc.h:341),
`Item::isSpellOrRune()` (used at npc.cpp:3278/4097), member `spellInfo`
(npc.h:597).

DEFERRED option: if maintainers prefer to avoid any change to the transform
trigger timing, leave as-is -- the divergence is unreachable from unmodified
Gothic II scripts. Recorded here for completeness with the original-behavior
citation.
