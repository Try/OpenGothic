# Transform-back revert omits PERC_ASSESSSURPRISE broadcast

**Confidence:** High

## Original fn + address
`oCSpell::EndTimedEffect` @0x00486e10 (`oSpell.cpp`) is the timed-effect terminator
for shapeshift / morph-into-creature spells (spell ids 0x2f..0x3a). After it has put the
original caster vob back into the world, restored its position/heading, copied the
transform-invariant values back via `oCNpc::CopyTransformSpellInvariantValuesTo`
@0x0073d3d0, searched a stand-ani, replayed the `T_TRFSHOOT_2_STAND` reverse-morph
animation and re-fired the visual FX, its *final* act on the successful-revert path is:

```
oCNpc::CreatePassivePerception(origCaster, 0x1e, origCaster, NULL)   // @0x0075b270
```

`CreatePassivePerception` is `__thiscall (this=npc, int percID, zCVob* other, zCVob* victim)`
(verified by decompiling 0x0075b270; same call the already-landed invest-caster note uses
with 0x1d=PERC_ASSESSCASTER). Here `percID = 0x1e = 30 = PERC_ASSESSSURPRISE`, with
OTHER = the restored caster itself and no VICTIM. The engine thus broadcasts a "surprise"
passive perception from the npc the instant it morphs back to its true form, so bystanders
that enabled `PERC_ASSESSSURPRISE` can react to a shapeshifter suddenly reappearing.

Both manual and automatic reverts route through this function: the change-level abort
(`oCTriggerChangeLevel::TriggerTarget` @0x0043be20, mirrored in
`game/world/triggers/zonetrigger.cpp:21`) ends the timed effect through the same path,
and OpenGothic's `Npc::transformBack()` is already documented as the mirror of
`EndTimedEffect` (zonetrigger NOTE @lines 17-20).

## OG file:line
`game/world/objects/npc.cpp:4839` — `Npc::transformBack()` (ends at line 4852,
`transformSpl.reset();`). It restores visual/body/talents/inventory and resets
`transformSpl`, but never sends any perception. Grep confirms `PERC_ASSESSSURPRISE`
(constants.h:439) is **never** emitted anywhere in `game/`, so the surprise reaction on
de-transform is entirely absent.

## Divergence
In the original, reverting a transform spell broadcasts PERC_ASSESSSURPRISE (30) from the
restored caster to nearby perceptive NPCs (its last action in `EndTimedEffect`). OpenGothic
restores the npc silently, so NPCs scripted to react to a shapeshifter morphing back never
receive the perception and stay idle.

## Proposed patch
`game/world/objects/npc.cpp`, end of `Npc::transformBack()`:

OLD:
```cpp
  invent.updateView(*this);
  transformSpl.reset();
  }
```
NEW:
```cpp
  invent.updateView(*this);
  transformSpl.reset();

  // NOTE: in original-game oCSpell::EndTimedEffect @0x00486e10 the final act of the
  // transform revert is oCNpc::CreatePassivePerception(self,0x1e,self,NULL) @0x0075b270 --
  // a PERC_ASSESSSURPRISE (30) broadcast from the restored caster (OTHER=self, no VICTIM),
  // so bystanders react with surprise when a shapeshifter morphs back to its true form.
  // OpenGothic restored the npc but never emitted the perception.
  owner.sendPassivePerc(*this,*this,PERC_ASSESSSURPRISE);
  }
```
`sendPassivePerc(Npc& self, Npc& other, int32_t perc)` (world.h:170) with self==other and
no victim exactly matches `CreatePassivePerception(self,0x1e,self,NULL)`; `PERC_ASSESSSURPRISE`
is defined in constants.h:439 and the enum is already used in this TU (e.g.
`PERC_ASSESSREMOVEWEAPON` @line 3933).
