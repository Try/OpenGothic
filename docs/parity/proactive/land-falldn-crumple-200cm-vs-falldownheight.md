# Land-anim selection: hard-land "crumple" gated by a hardcoded 200 cm drop, not by `falldownHeight()`

**Confidence:** Medium

## Original function + address

The NPC/player fall-animation state machine lives in the human animation controller
`oCAniCtrl_Human` and its base `zCAIPlayer` in `Gothic2.exe`.

- Fall onset is signalled by the physics layer through the virtual
  `zCAIPlayer::StartFallDownAni` (around address 0x00511da0) and the override
  `oCAniCtrl_Human::StartFallDownAni` (around 0x006b5220). Both unconditionally start the
  in-air falling state animation `S_FALLDN` (the override only substitutes the dead/unconscious
  variants). There is no height/speed test here: every plain gravity fall — walking off a
  ledge, releasing a hang, the descent half of a jump (the transitions `T_JUMP_2_FALLDN`,
  `T_JUMPUP_2_FALLDN`, `T_HANG_2_FALLDN` all target `S_FALLDN`) — uses `S_FALLDN` for the entire
  airborne portion.

- The hard-landing ("crumple to knees / lying") decision is made at touchdown by
  `oCAniCtrl_Human::CheckFallStates` (around 0x006b5810), reached from
  `CheckSpecialStates` (0x006b4290). When `S_FALLDN` is active and the body reaches the floor
  (height-above-ground drops below a near-ground epsilon of 10.0, constant `0x41200000`), the
  function compares the **accumulated vertical fall-drop** (the same scalar later handed to
  `oCNpc::CreateFallDamage`, 0x00681da0) against a **hardcoded 200.0** (constant `0x43480000`):
  - drop < 200 → return: a soft landing, the actor simply stands (the regular
    `T_FALLDN_2_STAND` path), no crumple.
  - drop ≥ 200 → play `T_FALLDN_2_FALL` or `T_FALLDN_2_FALLB` (direction chosen from the dot
    product of the model's at-vector with its velocity), i.e. enter the impact poses
    `S_FALL`/`S_FALLB`, which then transition into `S_FALLEN`/`S_FALLENB` (lying), then stand.

So in the original the airborne animation is always `S_FALLDN`; `S_FALL`/`S_FALLB` are only the
momentary touchdown-impact poses, and the crumple is gated by a fixed 200 cm *measured drop* at
the instant of landing — a constant that is independent of the guild `falldown_height` and of
the ACROBAT talent.

## OpenGothic file:line

- `game/game/movealgo.cpp:368-391` — the airborne anim selection.
- `game/world/objects/npc.cpp:1099-1100` — `Npc::isFallingDeep()`.
- `game/world/objects/npc.cpp:2248-2261` — `Npc::takeFallDamage()` landing branch.
- `game/game/movealgo.cpp:656-665` — `MoveAlgo::falldownHeight()`.

## Divergence

OpenGothic chooses the *airborne* animation from a predicted height and the per-guild
fall-down height, and then derives the crumple from that airborne animation:

```cpp
// movealgo.cpp
const float h0     = falldownHeight();             // guild falldown_height (x2 if ACROBAT)
float fallTime     = fallSpeed.y/gravity;
float height       = 0.5f*std::abs(gravity)*fallTime*fallTime;  // ~= distance already fallen
if(height>h0) {
  npc.setAnim(AnimationSolver::FallDeep);          // -> S_FALL / S_FALLB  (impact pose) IN AIR
  setState(Falling);
}
else if(fallSpeed.y<-0.3f && bs!=BS_JUMP && bs!=BS_FALL) {
  npc.setAnim(AnimationSolver::Fall);              // -> S_FALLDN
  setState(InAir);
}
```

```cpp
// npc.cpp Npc::isFallingDeep()
return (mvAlgo.isInAir() || mvAlgo.isFalling()) &&
       (visual.pose().isInAnim("S_FALL") || visual.pose().isInAnim("S_FALLB"));
// npc.cpp Npc::takeFallDamage()
if(!isFallingDeep()) setAnim(Anim::Idle);          // soft land
else                 setAnim(FallenA/FallenB);     // crumple
```

Two concrete differences follow:

1. **Airborne animation.** Once a plain gravity fall exceeds `falldownHeight()` (~200 cm by
   default), OpenGothic switches the in-air animation to the impact poses `S_FALL`/`S_FALLB`
   (`Anim::FallDeep`) and keeps them looping for the rest of the descent. The original keeps
   `S_FALLDN` in the air the whole way down and only touches `S_FALL`/`S_FALLB` for a single
   transition at touchdown.

2. **Crumple threshold source.** OpenGothic ties "crumple vs stand" to whether that airborne
   `S_FALL`/`S_FALLB` was selected, i.e. to `falldownHeight()` (guild `falldown_height`, and
   **doubled for ACROBAT**), evaluated from instantaneous vertical velocity at the tick the
   actor first crosses above-ground. The original gates the crumple on a **fixed 200 cm
   measured drop** at the moment of landing, independent of guild value and ACROBAT.

For the vanilla human guild (`falldown_height == 200`, no ACROBAT) the two largely coincide, so
the visible effect is mostly (a) the wrong looping airborne pose on falls taller than ~2 m and
(b) edge cases where the velocity-derived predicted height diverges from the true drop (launched
/ knocked-back falls, slope landings, brief velocity spikes that latch `Falling` early). The
threshold-source difference becomes overt only for non-default `falldown_height` guilds and for
ACROBAT actors (where OpenGothic needs a 400 cm fall to crumple while the original still crumples
at 200 cm) — but the ACROBAT doubling is an already-accepted parity decision (see
`talent-acrobat-falldown.md`) and is out of scope here.

## Proposed patch

DEFERRED.

Reasons:
- A faithful fix is not a one-liner: it must (a) keep the airborne animation as `Anim::Fall`
  (`S_FALLDN`) for plain gravity falls instead of switching to `Anim::FallDeep`, (b) move the
  crumple decision into the landing path (`Npc::takeFallDamage`) and gate it on the actual
  vertical drop crossing a hardcoded 200 cm rather than on `isFallingDeep()`, and (c) redefine
  `Npc::isFallingDeep()` / its other caller at `npc.cpp:3801` accordingly. That is a coordinated
  multi-site change to the fall-anim state machine with real regression surface against the
  existing knee-land and fall-damage paths.
- The precise in-air semantics of `S_FALL`/`S_FALLB` vs `S_FALLDN` (looping airborne loop vs
  touchdown-impact pose) should be confirmed against the actual humans `.MDS` before reworking,
  since the visual claim in difference (1) depends on it.

`// NOTE: in original-game oCAniCtrl_Human::CheckFallStates @0x006b5810 gates the S_FALLDN -> S_FALL/S_FALLB`
`// crumple on a hardcoded 200 cm measured fall-drop (const 0x43480000), evaluated at landing and`
`// independent of guild falldown_height / ACROBAT; the airborne anim is always S_FALLDN (StartFallDownAni @0x00511da0).`
