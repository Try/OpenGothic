# Hit-flinch suppressed during an interruptable mob-interaction (BS_MOBINTERACT_INTERRUPT)

**Confidence:** Medium-High (divergence is decompiler-verified; surgical fix is DEFERRED because it is
entangled with an already-deferred ladder case).

## Original fn + address (prose)

`oCNpc::OnDamage_Anim` (Gothic2.exe `0x00675bd0`) is the per-hit reaction builder. After resolving the
hit it decides whether to play the physical flinch/stumble. The gate (around the binary's
`IsBodyStateInterruptable` call site) is:

> play the flinch (`Interrupt(0,0)` -> `SetBodyState(BS_STUMBLE=0x15)` -> start the `T_..._HIT` /
> `S_..._HIT` interrupt animation) **iff** `IsBodyStateInterruptable()` is true **and** the victim is
> not a player holding a readied ranged weapon (`bVar24`) **and** a second non-player guard (`bVar22`)
> is false. Otherwise it routes a `T_GOTHIT` conversation message instead.

`oCNpc::IsBodyStateInterruptable` (`0x0075efa0`) returns true exactly when the `BS_FLAG_INTERRUPTABLE`
bit (`0x8000`) is set on the packed body-state word **and** no `BS_MOD_*` modifier bit (`& 0x3f80`) is
set. Crucially, `OnDamage_Anim` performs **no** `GetInteractMob()` / interaction check — it gates the
flinch purely on the interruptable flag. So an NPC whose current state is `BS_MOBINTERACT_INTERRUPT`
(`16 | BS_FLAG_INTERRUPTABLE`), i.e. inside the interruptable window of a mobsi animation (forge, anvil,
bench, ladder...), **does** get knocked into `BS_STUMBLE` and out of the mob when hit.

(For reference, the weapon-draw sibling gate `oCNpc::CanDrawWeapon` @ `0x006805c0` is already faithfully
mirrored by `Npc::canSwitchWeapon`.)

## OG file:line

`game/world/objects/npc.cpp:2225-2237` (inside `Npc::takeDamage(...)`):

```cpp
if(hitResult.hasHit) {
  auto state = bodyStateMasked();
  if(interactive()==nullptr && ((state&BS_FLAG_INTERRUPTABLE)!=BS_NONE || state==BS_RUN || state==BS_NONE)) {
    ...
    setAnimAngGet(lastHitType=='A' ? Anim::StumbleA  : Anim::StumbleB);
  }
}
```

This is the *only* hit-flinch path in OpenGothic; there is no separate mobsi-hit-interrupt elsewhere
(`grep` for interrupt/MOBINTERACT in `npc.cpp`/`interactive.cpp` shows only `visual.interrupt()` at
line 2232, which is inside this same `interactive()==nullptr` branch).

## Divergence

The original gates the flinch only on `IsBodyStateInterruptable()` (flag set, no modifier). OpenGothic
adds an extra `interactive()==nullptr` precondition. Consequently, when an NPC is hit during the
interruptable window of a mob interaction (`BS_MOBINTERACT_INTERRUPT`, which carries
`BS_FLAG_INTERRUPTABLE`), the original plays the flinch and knocks them into `BS_STUMBLE`, while
OpenGothic plays nothing and the victim stays glued to the mob animation for that frame window. The
sit/lie/sleep cases themselves are faithful: `BS_SIT` (`11|FREEHANDS`) and `BS_LIE` (`12`) lack the
interruptable flag, so neither engine flinches — both correct.

A second, lower-impact facet of the same root cause: `IsBodyStateInterruptable` also fails when any
`BS_MOD_*` modifier bit is set; OG's `bodyStateMasked()` strips all modifier bits, so an
interruptable-but-modified NPC would flinch in OG where the original would not. (Practically
unobservable unless an animation declares a modifier bs.)

## Proposed patch

**DEFERRED.** A faithful change would drop the `interactive()==nullptr` precondition and gate purely on
the interruptable flag (matching `IsBodyStateInterruptable`). That is not a safe surgical fix here:

1. It is entangled with the **already-deferred** `ladder-mid-climb-detach-fall` case — a ladder is a
   mobsi and `BS_CLIMB` (`9|BS_FLAG_INTERRUPTABLE`) also has `interactive()!=nullptr`, so removing the
   guard re-enables exactly that excluded behavior. Distinguishing "interruptable non-ladder mobsi"
   from "ladder" cleanly is the crux of the deferred item.
2. `visual.interrupt()` + `setAnimAngGet(StumbleA/B)` while `interactive()!=nullptr` would fire the
   stumble through the mobsi animation pipeline (the in-line `// TODO: put down in pipeline` already
   flags this path as fragile); not build-verifiable as safe without exercising the mobsi state
   machine.

The net visible difference (a one-shot flinch frame before the perception system already pulls the NPC
off the mob to fight) is minor, so the risk does not justify a blind change.

```
// NOTE: in original-game oCNpc::OnDamage_Anim @0x00675bd0 the flinch/stumble is gated only on
// oCNpc::IsBodyStateInterruptable @0x0075efa0 (BS_FLAG_INTERRUPTABLE set, no BS_MOD_* bit) with no
// GetInteractMob() check, so a hit during BS_MOBINTERACT_INTERRUPT flinches the victim out of the
// mobsi; OpenGothic's extra interactive()==nullptr guard suppresses it. DEFERRED: overlaps the
// deferred ladder-mid-climb-detach-fall case (BS_CLIMB is also an interactive mobsi).
```
