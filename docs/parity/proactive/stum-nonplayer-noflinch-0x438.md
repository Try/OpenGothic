# Stumble gate: non-player "no-flinch" field (oCNpc+0x438) — and the player-only nature of the stumble suppressors

**Confidence:** Low (DEFERRED — no actionable, build-verifiable fix; the divergence is inert in a normal session).

## Original function + address (prose)

`oCNpc::OnDamage_Anim` (Gothic2.exe `@0x00675bd0`, oNpc_Damage.cpp). After the
fly-vs-stumble selector, when a registered hit is routed to the "causes
stumble" path, the engine decides between a *soft* queued `T_GOTHIT`
`oCMsgConversation` reaction and the *hard* stumble
(`ClearEM` -> `oCNpc::Interrupt(this,0,0)` `@0x00735ab0` ->
`SetBodyState(BS_STUMBLE=0x15)` -> post the directional `T_..STUMBLE[B]`
transition). The hard stumble is taken only when **all** of three predicates are
false:

- `bVar24` (armed player): `IsAPlayer() && 1 <= GetWeaponMode() <= 7`.
- `IsBodyStateInterruptable() == 0`.
- `bVar22` (non-player no-flinch): `!IsAPlayer() && *(int*)(this+0x438) != 0`.

The leading virtual call in both `bVar24`/`bVar22` is `*(*this+0x100)()`. I
resolved this concretely: the oCNpc vtable base is `0x0083d724`; slot `+0x100`
(`0x0083d824`) is `oCNpc::IsAPlayer` (`@0x007425a0`, body `return this==player`),
and `+0x110` (`0x0083d834`) is `oCNpc::IsHuman` (`@0x00742640`). So **both
stumble suppressors are keyed on IsAPlayer, not IsHuman** — the armed-stumble
guard (`bVar24`) is *player-only*, and the no-flinch field gate (`bVar22`)
applies only to *non-player* NPCs.

The same field `this+0x438` gates the animation interrupt itself: in
`oCNpc::Interrupt` (`@0x00735ab0`) `HitInterrupt` on the human ani-controller is
only invoked `if (*(int*)(this+0x438) == 0)`, and `oCNpc::OnDamage_Events`
(`@0x0067abe0`) only calls `StopSelectedSpell` `if (IsAPlayer()==0 &&
*(this+0x438)==0)`. So a non-player with `0x438 != 0` neither hit-interrupts nor
hard-stumbles — a genuine "no-flinch" state, conceptually the kind of behaviour
wanted for warrior/troll big-monster hit absorption.

## OpenGothic file:line

`game/world/objects/npc.cpp:2199-2213` (`Npc::takeDamage`, the stumble block).
OpenGothic models neither suppressor's non-bodystate inputs: it stumbles on any
`hitResult.hasHit` that passes the bodystate gate, with no `IsAPlayer()`
discrimination and no analogue of `oCNpc+0x438`.

## Divergence (and why DEFERRED)

The player-armed half (`bVar24`) is already covered by
`bsint-player-weapon-stumble-guard.md` / `hit-stumble-weapon-drawn.md`; this
note resolves the open question those docs flagged — the gate is **IsAPlayer**,
so monsters in Fist mode are *not* affected and there is no monster-combat
regression risk in that suppressor.

The genuinely-uncovered half is the non-player `0x438` no-flinch gate. It is
**DEFERRED with no patch** because the field is inert in a normal session:

- grep over every function that touches struct offset `0x438` (warm-decompiler
  `offsets 0x438`) shows the only oCNpc writers are `oCNpc::CleanUp`
  (`@0x0072e410`, sets it to `0`) and `oCNpc::Unarchive` (`@0x00747230`,
  restores the saved value). `oCNpc::Archive` (`@0x00746470`) persists it;
  `OnDamage_Anim`, `OnDamage_Events`, `Interrupt` only read it.
- No code path sets it non-zero; `oCNpc::CopyTransformSpellInvariantValuesTo`
  (`@0x0073d3d0`) copies the adjacent `+0x434`, not `+0x438`. So the field is
  effectively always `0`, `bVar22` is always false, and the non-player no-flinch
  branch is dead in practice. Implementing it in OpenGothic would have no
  observable effect, so there is nothing to faithfully reproduce.

No high-confidence, surgical, build-verifiable behavioral fix in the
stumble/flinch subsystem was found beyond the already-fixed/deferred set; the
remaining original-engine stumble gates (`poStack_e8<2`, the pending
`oCMsgWeapon`/`oCMsgAttack` queue scan driving `uStack_104`, the `oSDamageDescriptor+0x90 & 0xc`
flags) are tied to the `oCMsgConversation` event-manager queue architecture that
OpenGothic does not replicate, and are not surgically portable.

// NOTE: in original-game oCNpc::OnDamage_Anim @0x00675bd0 the hard-stumble gate is
// !(IsAPlayer()&&weaponMode in 1..7) && IsBodyStateInterruptable() && !(!IsAPlayer() && *(this+0x438)).
// vtable slot +0x100 == oCNpc::IsAPlayer @0x007425a0 (vtable base 0x0083d724). Field +0x438
// also gates HitInterrupt in oCNpc::Interrupt @0x00735ab0; it is only zeroed (CleanUp) /
// save-restored (Un/Archive), never set non-zero, so the non-player no-flinch branch is inert.
