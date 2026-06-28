# Non-projectile spell commit sends PERC_ASSESSMAGIC unconditionally, ignoring the VFX `sendAssessMagic` flag

**Confidence:** Medium-High

## Original function + address (prose)

In the original engine the "you cast magic at me" perception (`PERC_ASSESSMAGIC`,
delivered through `oCNpc::AssessMagic_S` @ `0x0075cc30`) is raised inside
`oCVisualFX::Init` @ `0x00491f20`. There the engine first checks that the effect's
target vob (`oCVisualFX` field `0x4a8`) is an `oCNpc`, then gates the perception on the
VFX's *sends-assess-magic* flag (`oCVisualFX` field `0x380`, written by
`oCVisualFX::SetSendsAssessMagic` @ `0x0048b2c0` / read by `GetSendsAssessMagic`
@ `0x0048b350`), and finally requires that the target NPC is **not** the caster
(`this_00 != poVar16`, i.e. target `0x4a8` differs from origin `0x4ac`). So a directed
spell only makes an NPC assess hostile magic when (a) the spell's effect actually has an
NPC target, (b) that effect's VFX is authored with `sendAssessMagic` set, and (c) the
target is not the caster. Self-buff / utility effects (Light, Heal, summons, etc.) whose
VFX clear that flag never trigger the perception, and the caster is never notified about
its own spell.

OpenGothic already encodes exactly this gating for the *projectile/collision* path in
`Effect::onCollide` (`game/graphics/effect.cpp:320-323`): it picks
`emFXCollDynPerc` (Gothic 2) or `emFXCollDyn` (Gothic 1) as the "perc" sub-VFX and only
sends `PERC_ASSESSMAGIC` when `sender->sendAssessMagic` is true.

## OG file:line

`game/world/objects/npc.cpp:3488-3491` (the non-projectile branch of
`Npc::commitSpell()`):

```cpp
visual.setMagicWeaponKey(owner,SpellFxKey::Init);
if(currentTarget!=nullptr) {
  currentTarget->lastHitSpell = splId;
  currentTarget->perceptionProcess(*this,nullptr,0,PERC_ASSESSMAGIC);
  }
```

## Divergence

For every non-shoot spell, OpenGothic sets `currentTarget->lastHitSpell` and broadcasts
`PERC_ASSESSMAGIC` to the live focus NPC unconditionally — it never consults the spell
VFX's `sendAssessMagic` flag, never checks `currentTarget!=this`, and fires even when the
spell has no VFX. In the original, the very same perception is gated on the VFX flag and
the not-self check (and OpenGothic's own collision path honors the flag). Consequence: a
focused NPC perceives a harmless self/utility spell (Light, Heal, summon, telekinesis —
whose VFX clear `send_assess_magic`) as hostile magic, recording `lastHitSpell` and
running its `ZS_AssessMagic` reaction, where the original NPC would not react at all.

## Proposed patch

```cpp
// OLD
    visual.setMagicWeaponKey(owner,SpellFxKey::Init);
    if(currentTarget!=nullptr) {
      currentTarget->lastHitSpell = splId;
      currentTarget->perceptionProcess(*this,nullptr,0,PERC_ASSESSMAGIC);
      }
    }

// NEW
    visual.setMagicWeaponKey(owner,SpellFxKey::Init);
    // NOTE: in original-game oCVisualFX::Init @0x00491f20 the ASSESSMAGIC perception
    // (oCNpc::AssessMagic_S @0x0075cc30) is raised only when the effect's VFX has its
    // sendsAssessMagic flag set (oCVisualFX field 0x380, SetSendsAssessMagic @0x0048b2c0)
    // and the target NPC is not the caster. OpenGothic's collision path (Effect::onCollide)
    // already honors this flag via emFXCollDynPerc (G2) / emFXCollDyn (G1); this
    // non-projectile commit branch sent it unconditionally, so bystanders/foci perceived
    // harmless self-buffs (Light/Heal/summon) as hostile magic.
    const bool      g2   = owner.version().game==2;
    const VisualFx* perc = (vfx==nullptr) ? nullptr : (g2 ? vfx->emFXCollDynPerc : vfx->emFXCollDyn);
    if(currentTarget!=nullptr && currentTarget!=this && perc!=nullptr && perc->sendAssessMagic) {
      currentTarget->lastHitSpell = splId;
      currentTarget->perceptionProcess(*this,nullptr,0,PERC_ASSESSMAGIC);
      }
    }
```

`vfx` is the `const VisualFx*` already fetched at the top of this `else` block
(`owner.script().spellVfx(splId)`); `emFXCollDyn`, `emFXCollDynPerc` and `sendAssessMagic`
are public members of `VisualFx` (`game/graphics/visualfx.h:136-151`), exactly as used by
`Effect::onCollide`.

### Residual uncertainty
The original raises ASSESSMAGIC from `oCVisualFX::Init`, checking the flag on whichever
VFX is being initialized; mapping that to `emFXCollDynPerc`/`emFXCollDyn` mirrors
OpenGothic's existing canonical handling in `Effect::onCollide` rather than the literal
field-`0x380` VFX, so the exact sub-VFX selected could differ for an unusual spell. The
core defect — the flag/self gate being absent entirely on this path — is certain.
