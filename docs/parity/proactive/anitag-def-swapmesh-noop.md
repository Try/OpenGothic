# Anim event-tag parity: `DEF_SWAPMESH` is dispatched but never executed

**Confidence:** High that this is a behavioral divergence (the event is a verified
no-op in OpenGothic while the original performs a real slot swap). The fix is
**DEFERRED** — see reason below — so confidence in a *surgical, build-verifiable
patch* is deliberately not asserted.

## Original function + address (prose)

In `Gothic2.exe`, the per-frame anim event-tag dispatcher
`oCNpc::DoDoAniEvents` (entry `0x00742a20`) walks the model's freshly-activated
`zCModelAniEvent` list and string-compares each tag name. When the tag equals
`DEF_SWAPMESH` it extracts the event's **two** string arguments (slot-name #1 and
slot-name #2 — read from the event record at member offsets `+8` and `+0xd`) and
invokes the virtual `oCNpc::DoModelSwapMesh(const zSTRING&, const zSTRING&)`
(entry `0x00743dc0`, vtable slot `+0xdc`).

`DoModelSwapMesh` is not cosmetic-only: it (1) looks up the two NPC equipment
slots (`TNpcSlot`) by name in the NPC's slot list, (2) detaches the item-vob
currently held in each slot, (3) re-attaches them **swapped** via `PutInSlot`
(the slot's bone/attachment is reassigned to the other slot's vob), and (4) when
the affected slot is a fight slot, re-runs the weapon/fight-mode refresh
(`oCAniCtrl_Human::SetFightAnis` / `SetWalkMode`). In short, `DEF_SWAPMESH`
exchanges the *visual meshes attached to two named slots* at the event's frame.

The frame-trigger plumbing itself is correct in OpenGothic; the divergence is in
the **dispatch action**, not the trigger timing.

## OpenGothic file:line

- `game/graphics/mesh/animation.cpp:507` — `Animation::Sequence::processEvent`
  correctly packs the event into an `EvTimed` for `MESH_SWAP`, copying both slot
  names: `ex.slot[0] = e.slot; ex.slot[1] = e.slot2;` (grep-verified).
- `game/world/objects/npc.cpp:2389` — in the `ev` dispatch switch:
  `case zenkit::MdsEventType::MESH_SWAP: break;` (no-op).
- `game/world/objects/npc.cpp` `Npc::tickTimedEvt` (the `ev.timed` loop) — the
  `case zenkit::MdsEventType::MESH_SWAP: break;` arm is likewise empty
  (grep-verified: the only two `MESH_SWAP` sites outside `animation.cpp` are both
  bare `break;`).

So the event is parsed, time-stamped, sorted into `ev.timed`, and then silently
dropped. Both slot names survive all the way to the handler and are discarded.

## Divergence

A `DEF_SWAPMESH(slotA slotB)` event tag has **no effect** in OpenGothic. In the
original, at that frame the meshes/items attached to `slotA` and `slotB` are
exchanged (and, for fight slots, the fight-mode visuals refresh). Any animation
that relies on `DEF_SWAPMESH` to reposition or swap an equipped visual mid-anim
will look wrong in OpenGothic (the mesh stays in its original slot).

Verified that this is a true gap and not handled elsewhere: the only OpenGothic
references to `MESH_SWAP` / `SWAPMESH` are the three sites above
(`grep -rni "swapmesh|MESH_SWAP"` over `game/`), none of which mutate the visual.
The neighbouring morph event (`mmStartAni` → `EvMorph` → `MdlVisual::startMMAnim`)
*is* implemented; mesh-swap is the only member of the morph/mesh-swap family left
unhandled.

Also checked and explicitly **excluded** as findings (to avoid false positives):
- `DEF_PAR_FRAME` (`PARRY_FRAME` → `Animation::AnimData::defParFrame` /
  `Sequence::isDefParWindow`): OpenGothic computes it but never calls
  `isDefParWindow` — i.e. dead code. The original parses `DEF_PAR_FRAME` into
  `oCAniCtrl_Human+0x1a8/0x1ac` in `GetFightLimbs` (`0x006af1e0`) but **no runtime
  combat function reads those fields** (only `StartHitCombo` `0x006b00c0` and
  `GetFightLimbs` write them; `HitCombo`/`CanParade`/`HitInterrupt`/
  `StartParadeEffects` never read them). So the par-frame window is vestigial in
  Gothic 2 as well — OpenGothic's unused field *matches* the original. No divergence.
- The melee hit gate (`DEF_OPT_FRAME` → `Npc::commitDamage`) omitting a height
  check: already covered — `FightAlgo::qDistTo` returns `1e30` when
  `!fightSameHeight(...)` and uses XZ-only distance, mirroring
  `oCNpc::IsInFightRange`/`IsSameHeight` (existing NOTE in `fightalgo.cpp`).

## Proposed patch

**DEFERRED.**

Reason: OpenGothic's visual-attachment API (`MdlVisual::setSlotItem` /
`clearSlotItem` in `game/graphics/mdlvisual.h:53-55`) is keyed by *bone* name and
takes ownership of a `MeshObjects::Mesh`, whereas `DEF_SWAPMESH` operates on the
NPC's *named equipment slots* (`TNpcSlot`) and swaps the **item-vobs** between
them, additionally re-running fight-mode/weapon refresh for fight slots. There is
no existing OpenGothic primitive that (a) reads the current item-visual held in a
named NPC slot and (b) re-homes it into another named slot without going through
inventory re-equip side effects. A faithful, low-risk reimplementation needs a
small slot-to-slot visual-swap helper on `MdlVisual`/`Inventory` first; bolting
the swap onto the empty `case MESH_SWAP` without that primitive risks
weapon-mode/equip corruption. Impact is low-frequency (few G2 anims use
`DEF_SWAPMESH`), so this is recorded as a known gap rather than force-fitted into
a surgical edit.

// NOTE: in original-game oCNpc::DoDoAniEvents @0x00742a20 a DEF_SWAPMESH event tag
// calls oCNpc::DoModelSwapMesh @0x00743dc0, which exchanges the item-vobs attached
// to the two named NPC slots (PutInSlot swap) and refreshes fight visuals;
// OpenGothic parses the event (animation.cpp:507) but the MESH_SWAP dispatch arms
// (npc.cpp:2389 and Npc::tickTimedEvt) are no-ops.
