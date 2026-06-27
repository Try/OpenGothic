# ctrl: sneak/crouch toggle is wrongly gated on the learnable SNEAK talent

**Confidence:** High

## Original function + address
`oCAIHuman::PC_Sneak` (Gothic2.exe @ `0x0069a790`) is the player-control handler for the
sneak (crouch) toggle key. Its gating is, in order: an "uninterruptible animation" guard
(virtual call at oCNpc vtable +0x104 combined with model flag at +0x1f8 bit 1), a key-pressed
guard, a stray CapsLock reset, and then a requirement that the controller is in the
*Walking* or *Standing* animation state (`oCAniCtrl_Human::IsWalking` / `IsStanding`) before
flipping walk-mode via `CanToggleWalkModeTo`/`ToggleWalkMode`. Crucially, **there is no test
of the SNEAK talent anywhere in this function** — crouch is available to the player from the
start of the game.

The companion routine `oCAIHuman::CreateFootStepSound` (@ `0x0069b180`) confirms how the
*silent* benefit works in the engine: when walk-mode == 2 (sneak) the function early-outs and
emits **no** quiet-sound perception (`oCNpc::AssessQuietSound_S`); only non-sneak walking
emits it. So the "footsteps are silent while crouching" behaviour is keyed purely on the
sneak walk-mode and is likewise talent-independent in C++ (the SNEAK talent's effect lives in
the perceiving-NPC scripts, not in the crouch/footstep engine code). OpenGothic already
mirrors this footstep-suppression in `Npc::tickAnimationTags` keyed off `WM_Sneak` (the #639
fix), so the engine half is consistent — only the *toggle* is over-gated.

## OpenGothic file:line
`game/game/playercontrol.cpp:430-431` (`PlayerControl::toggleSneakMode`):

```cpp
auto pl = w->player();
if(pl->canSneak())
  pl->setWalkMode(pl->walkMode()^WalkBit::WM_Sneak);
```

with `Npc::canSneak()` at `game/world/objects/npc.cpp:1230`:

```cpp
bool Npc::canSneak() const {
  return talentSkill(TALENT_SNEAK)!=0;
  }
```

## Divergence
OpenGothic refuses to enter sneak/crouch mode unless the player has learned the *Sneaking*
talent (`talentSkill(TALENT_SNEAK)!=0`). In the original game the crouch toggle has no talent
requirement at all — the player can crouch-walk from the very first minute. The learnable
talent only governs whether nearby NPCs/monsters notice you (a script-side perception
concern), not whether the body can crouch. Result: a fresh / talentless OpenGothic player
cannot crouch, whereas the same player in Gothic2.exe can. `canSneak()` is referenced only
here (player toggle), so the fix is local to the player and cannot affect NPCs.

## Proposed patch
Drop the talent gate so the crouch toggle matches `PC_Sneak`'s talent-independent behaviour.

OLD (`game/game/playercontrol.cpp`):
```cpp
  auto pl = w->player();
  if(pl->canSneak())
    pl->setWalkMode(pl->walkMode()^WalkBit::WM_Sneak);
```

NEW:
```cpp
  auto pl = w->player();
  // NOTE: in original-game oCAIHuman::PC_Sneak @0x0069a790 the crouch toggle has NO SNEAK-talent
  // test (it gates only on the Standing/Walking animation state); the learnable talent governs
  // NPC perception, not the ability to crouch. Footstep silence is likewise keyed on the sneak
  // walk-mode (oCAIHuman::CreateFootStepSound @0x0069b180), matching tickAnimationTags. So allow
  // the crouch toggle regardless of talent.
  pl->setWalkMode(pl->walkMode()^WalkBit::WM_Sneak);
```

Grep-verified symbols: `PlayerControl::toggleSneakMode`, `Npc::canSneak`, `Npc::walkMode`,
`Npc::setWalkMode`, `WalkBit::WM_Sneak` (constants.h:222) all exist. `canSneak()` has no other
caller, so it can be left in place (now unused by the toggle) or removed separately.

Note (optional refinement, NOT required for parity of the reported bug): the original also
restricts the toggle to the Standing/Walking states. OpenGothic does not model that gate here
and toggling mid-jump/swim is an edge case handled downstream by the animation solver; gating
on it would be feel-tuning and is left DEFERRED.
