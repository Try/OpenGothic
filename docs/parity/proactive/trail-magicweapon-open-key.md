# Magic-weapon readied glow uses INIT key instead of OPEN key

**Confidence:** Medium-High

## Original fn + address

When a spell/rune is *readied* in the original game, `oCMag_Book::Spell_Open`
(@0x004778f6) forwards to `oCSpell::Open` (@0x004858a0), which invokes the
spell's visual-FX vtable slot 0x70 = `oCVisualFX::Open` (@0x004918e0). `Open`
runs only while the FX life-state field `*(this+0x3b8) < 1` (i.e. a freshly
created/idle FX): it calls `InitEffect`, then searches the emitter-key array
(`this+1000`) for the key whose name ends in `_KEY_OPEN` (string suffix
`"OPEN"`, joined via `_KEY_` @0x0089723c). If found it makes that the active
emitter key (`*(this+0x4e0)`) and calls `UpdateFXByEmitterKey`; if **not**
found it leaves the FX showing its base/created visual. It then propagates
`Open` to child FX and sets state `*(this+0x3b8) = 1`.

The `INIT` key is a *later* lifecycle stage: `oCVisualFX::Init(vob,vob,vob)`
(@0x00491f20) applies the `_KEY_INIT` key (string `s_INIT`) and advances state
to `*(this+0x3b8) = 2`. `Init` is reached through `oCSpell::Setup` (@0x00484930,
called from `Spell_Setup`/`Spell_Cast`/`MagicMode`/`SetFrontSpell`), i.e. once
the cast sequence begins — not at ready time. So the engine clearly separates
two visuals: `OPEN` = readied in-hand idle glow, `INIT` = cast build-up.

## OG file:line

`game/game/inventory.cpp:582` (`Inventory::updateRuneView`):

```cpp
owner.setMagicWeapon(Effect(*vfx,owner.world(),owner,SpellFxKey::Init));
```

`SpellFxKey::Open` is declared in `game/game/constants.h:279` and is parsed in
`game/graphics/visualfx.cpp:113` (`_KEY_OPEN`), but `grep -rn "SpellFxKey::Open"
game/` returns **zero** call sites — the enum value is dead code.

## Divergence

`updateRuneView` is OpenGothic's "spell readied" moment (it builds the in-hand
magic-weapon FX via `MdlVisual::setMagicWeapon`). The original applies the
`OPEN` key here; OpenGothic applies `INIT`. For any spell FX that defines a
distinct `_KEY_OPEN` (idle glow) versus `_KEY_INIT` (cast build-up), the
readied weapon shows the wrong, cast-build-up appearance the moment the rune is
equipped, and `SpellFxKey::Open` is never exercised at all. (Where the FX has no
`_KEY_OPEN`, the original keeps the base visual; OpenGothic instead shows
`_KEY_INIT` if that exists — still a mismatch.)

The post-emit resets at `game/world/objects/npc.cpp:3433` and `:3445`
(`setMagicWeaponKey(owner,SpellFxKey::Init)`) restore the same readied-idle
glow after a cast and have the identical mismatch; they should track whatever
the ready key is, for visual consistency across recasts.

## Proposed patch

Primary, surgical (the unambiguous ready-time site):

```cpp
// OLD  game/game/inventory.cpp:582
owner.setMagicWeapon(Effect(*vfx,owner.world(),owner,SpellFxKey::Init));

// NEW
// NOTE: in original-game oCSpell::Open @0x004858a0 -> oCVisualFX::Open @0x004918e0
// applies the FX's _KEY_OPEN (readied/idle glow) when a spell is readied, not _KEY_INIT
// (oCVisualFX::Init @0x00491f20, the cast build-up stage reached via oCSpell::Setup).
owner.setMagicWeapon(Effect(*vfx,owner.world(),owner,SpellFxKey::Open));
```

For consistency the two post-cast resets in `npc.cpp` (lines 3433, 3445) should
likewise use `SpellFxKey::Open` so the weapon returns to the readied-idle glow
rather than the cast build-up visual after each cast.

Residual uncertainty (why Medium-High, not High): whether stock Gothic II spell
FX instances define a `_KEY_OPEN` distinct from `_KEY_INIT` is not verified from
script here; the engine semantics and the unused `SpellFxKey::Open` enum value
are the basis for the finding. If no stock spell defines a distinct `_KEY_OPEN`,
the change is visually inert (both fall back to base) and still correctness-
preserving.
