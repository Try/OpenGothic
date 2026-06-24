# Spell-FX invest-level overflow reverts to base FX instead of holding the last INVEST key

**Confidence:** High

## Original function + address
`oCVisualFX::InvestNext` (P:\dev\g2addon\release\Gothic\_carsten\oVisFx.cpp), entry **0x00492830**.

On each invest step the original advances an invest-level counter packed into the
bitfield at `this+0x55c` (the level occupies 5 bits, read as `(field<<0xe)>>0x1b`,
i.e. 0..31). It clamps the level at 20 (the `if (0x14 < level)` branch). It then
builds the key name `<instance>_KEY_INVEST_<level>` and resolves it by *name* via the
FX-key search helper (`FUN_00492680`, the by-name FindFXKey). The resolved array index
is returned in `iVar11`:

- If a key with that name is found (`iVar11 != -1`), it sets the active emitter key
  `this+0x4e0 = keys[iVar11]` and calls `UpdateFXByEmitterKey` (swaps PFX / sound /
  light / collision to that invest key).
- If **no** key with that name is found (`iVar11 == -1`), the original **skips the
  update entirely** — `this+0x4e0` (the currently-active emitter key) is left
  unchanged. The visual therefore keeps displaying the *last valid* INVEST key's
  appearance; it does NOT fall back to the spell's base/default visual.

`oCVisualFX::SetByScript` @0x0048d4b0 confirms the per-key load sequence
(`_KEY_OPEN`, `_KEY_INIT`, `_KEY_CAST`, `_KEY_STOP`, `_KEY_COLLIDE`, then a loop over
`_KEY_INVEST_<n>`), all stored in the same emitter-key array indexed by name.

## OpenGothic file:line
- `game/graphics/visualfx.cpp:149` — `VisualFx::key(SpellFxKey type, int32_t keyLvl)`
- `game/graphics/effect.cpp:217` — `Effect::setKey(...)` (consumer)
- Invest driver: `game/world/objects/npc.cpp:4075`
  `visual.setMagicWeaponKey(owner,SpellFxKey::Invest,castLvl+1);` with `castLvl` allowed
  to climb to 15 (npc.cpp:4073), i.e. invest level 1..15.

## Divergence
`VisualFx::key()` resolves the INVEST key purely by *vector index*:

```cpp
if(type==SpellFxKey::Invest && keyLvl>0) {
    keyLvl--;
    if(size_t(keyLvl)<investKeys.size())
      return &investKeys[size_t(keyLvl)];
    return nullptr;                 // <-- overflow
    }
```

When the requested invest level exceeds the number of `_KEY_INVEST_*` entries the
spell actually defines (common: a spell defines 3-4 invest keys but the caster keeps
investing up to level 15), `key()` returns `nullptr`. `Effect::setKey` then sets
`key=nullptr` and re-runs `setupPfx`/`setupSfx`/`setupLight`/`setupCollision`, which all
fall back to the *root* defaults (`root->visName_S`, `root->sfxID`,
`root->lightPresetName`). The on-weapon spell visual visibly **snaps back to its base
appearance** at high invest levels, whereas the original holds the last invest key's
FX. This is a behavioral (not purely cosmetic) divergence in the per-key PFX/sound/light
swap of the invest progression.

## Proposed patch
Clamp the invest index to the last available INVEST key instead of returning `nullptr`,
mirroring the original's "keep the previously-resolved invest key when the next one is
absent" behavior.

`game/graphics/visualfx.cpp`, in `VisualFx::key`:

OLD:
```cpp
  if(type==SpellFxKey::Invest && keyLvl>0) {
    keyLvl--;
    if(size_t(keyLvl)<investKeys.size())
      return &investKeys[size_t(keyLvl)];
    return nullptr;
    }
```

NEW:
```cpp
  if(type==SpellFxKey::Invest && keyLvl>0) {
    // NOTE: in original-game oCVisualFX::InvestNext @0x00492830, when the
    // <instance>_KEY_INVEST_<level> key is not found the active emitter key is left
    // unchanged, so the FX holds the last defined invest key rather than reverting to
    // the spell base visual. Clamp to the last available INVEST key to match.
    if(investKeys.empty())
      return nullptr;
    size_t idx = size_t(keyLvl-1);
    if(idx>=investKeys.size())
      idx = investKeys.size()-1;
    return &investKeys[idx];
    }
```

Symbols grep-verified to exist: `VisualFx::key` (visualfx.cpp:149), `investKeys`
(`std::vector<Key>`, visualfx.h:170, populated visualfx.cpp:137), `SpellFxKey::Invest`
(constants.h:282).

### Caveat / scope note
The original resolves invest keys by *name* (`_KEY_INVEST_<n>`), so it tolerates gaps in
the numbering (e.g. INVEST_1, INVEST_3 with no INVEST_2). OpenGothic's loader
(`visualfx.cpp:130-138`) `break`s at the first missing index, so it cannot represent
gaps. That is a separate, lower-impact loader divergence and is left DEFERRED here; the
clamp above is the surgical, build-safe fix for the dominant overflow case.
