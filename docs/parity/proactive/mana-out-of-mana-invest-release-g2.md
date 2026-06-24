# Mana: out-of-mana during invest does not auto-release the spell in Gothic 2

**Confidence:** High (for the player-control auto-release path); the engine field/symbol mapping is fully verified, the only soft spot is dependence on the script's `Spell_ProcessMana_Release` returning `SPL_SENDCAST` (standard G2 behaviour).

## Original function + address

`oCSpell::Invest` (`Gothic2.exe` @ 0x004850d0), invoked every frame for the
selected invest-spell via `oCMag_Book::Spell_Invest` (@0x00476530).

At the very top of the per-frame invest tick, before any time accumulation, the
original performs a version-agnostic mana gate:

- It reads the caster's mana attribute via `oCNpc::GetAttribute(caster, ATR_MANA)`
  (the attribute index is stored in the spell object and initialised to `2` =
  `ATR_MANA` by `oCSpell::InitValues` @0x00484020).
- If the caster pointer is null **or** mana is `< 1` (i.e. zero), and at least one
  unit of mana has already been invested (the invested-mana counter, init `0`),
  it calls `oCSpell::SetReleaseStatus` (@0x00486670) and returns.

`SetReleaseStatus` runs the script function `Spell_ProcessMana_Release` (not the
normal `Spell_ProcessMana`) and then coerces the resulting spell status: anything
other than `SPL_SENDCAST` (2) or `SPL_SENDSTOP` (3) is forced to `SPL_SENDSTOP`.
In the standard G2 scripts `Spell_ProcessMana_Release` returns `SPL_SENDCAST`, so
the accumulated invest spell **fires at its current level**.

There is no game-version branch anywhere in `oCSpell::Invest`; the binary
inspected is the G2 addon build, so this auto-release path is active in Gothic 2.
Net effect in the original: if the player keeps the cast button held while mana
drops to zero mid-invest, the spell is automatically released (cast) at whatever
level was reached.

## OpenGothic file:line

`game/game/playercontrol.cpp:713`

```cpp
if(!actrl[ActForward] || (Gothic::inst().version().game==1 && pl.attribute(ATR_MANA)==0)) {
  casting = false;
  pl.endCastSpell(true);
  }
```

`Npc::endCastSpell(true)` (`game/world/objects/npc.cpp:4097`) is the matching
release path: it calls `invokeManaRelease` (`Spell_ProcessMana_Release`) and casts
on `SPL_SENDCAST`, else finalizes — i.e. exactly the original `SetReleaseStatus`
semantics. `Npc::tickCast` (`game/world/objects/npc.cpp:4054`) has no out-of-mana
short-circuit; it just calls `invokeMana` (`Spell_ProcessMana`) every tick.

## Divergence

OpenGothic only auto-releases an invest spell on zero mana for **Gothic 1**
(`version().game==1 && attribute(ATR_MANA)==0`). For **Gothic 2**, while the
player still holds the forward/cast key, OG never reaches the release path: it
keeps ticking `tickCast` -> `invokeMana` (`Spell_ProcessMana`) at zero mana and
leaves the decision to the normal-cast script hook rather than the
release hook. The original drops out of mana and calls `Spell_ProcessMana_Release`
(via `SetReleaseStatus`) regardless of game version, so in G2 the held spell is
auto-cast at the reached level instead of stalling/aborting through the
non-release path.

## Proposed patch

Drop the `game==1` qualifier so the zero-mana auto-release fires for both games,
matching the version-agnostic gate in `oCSpell::Invest`.

OLD (`game/game/playercontrol.cpp:712-716`):
```cpp
  if(casting) {
    if(!actrl[ActForward] || (Gothic::inst().version().game==1 && pl.attribute(ATR_MANA)==0)) {
      casting = false;
      pl.endCastSpell(true);
      }
    return;
    }
```

NEW:
```cpp
  if(casting) {
    // NOTE: in original-game oCSpell::Invest @0x004850d0 the per-frame invest tick
    // releases the spell (SetReleaseStatus @0x00486670 -> Spell_ProcessMana_Release)
    // whenever GetAttribute(caster,ATR_MANA) < 1 and mana was already invested.
    // This gate has no game-version branch, so it applies to Gothic 2 as well.
    if(!actrl[ActForward] || pl.attribute(ATR_MANA)==0) {
      casting = false;
      pl.endCastSpell(true);
      }
    return;
    }
```

Notes on safety: `Npc::endCastSpell` already early-returns when `castLevel` is
not in the invest range, mirroring the original's "only release if mana was
invested" guard, so releasing on the very first frame (before any invest) is a
no-op. The release dispatch in `endCastSpell` (`SPL_SENDCAST` -> cast, else
finalize) already matches `SetReleaseStatus`'s status coercion.

If reviewers consider the script-side `Spell_ProcessMana_Release` return value
too uncertain to rely on for G2, treat as DEFERRED with that reason; the engine
field mapping above is nonetheless verified.
