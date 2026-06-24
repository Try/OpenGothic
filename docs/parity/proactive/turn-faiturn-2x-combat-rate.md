# NPC combat turn-to-enemy (faiTurn) rotates at 2x guild turn_speed; original AI turns at 1x

**Confidence:** Medium (hard decompiled ground truth for the rate factor; flagged feel-tuning because the OG author's `*= 2.f` is an explicit deliberate deviation, so the patch is proposed but marked DEFERRED-leaning).

## Original function + address (prose)

In `Gothic2.exe` the per-frame turn step is always `guildTurnSpeed * frameDeltaMs`,
with no in-combat multiplier on the *AI* path:

- `oCAniCtrl_Human::SetScriptValues` @0x006A5110 seeds the NPC field at `+0x49C`
  to `guildScriptTurnSpeed * 0.001` (the constant is the float `0x3A83126F` = 0.001),
  i.e. the guild `turn_speed` (deg/s, ~90 for stock G2 humans) stored in deg/ms.
- `oCNpc::GetTurnSpeed` @0x00680970 returns that `+0x49C` field, or the fixed
  `0x3DCCCCCD` (= 0.1 deg/ms = 100 deg/s) **only** when the colliding predicate
  (vtable slot `+0x100`) is set (the obstacle-avoidance cap, handled separately).
- `oCNpc::Turning` @0x00683120 and `oCNpc::EV_TurnToVob` @0x00686160 (the AI
  turn-to-target / turn-to-vob handlers) compute `step = turnSpeed * DAT_0099b3ec`
  (DAT_0099b3ec = frame delta in ms), clamp the normalized signed delta from
  `oCNpc::GetAngles` @0x006812B0 (folded to `[-180,180]`) to `±step`, then
  `oCAniCtrl_Human::TurnDegrees` @0x006AEB10 plays t_turnl/t_turnr and rotates
  by exactly that clamped amount. **Factor is 1x.**
- The only x4 multiplier (`step = GetTurnSpeed * frameMs * 0x40800000` (=4.0)) lives
  in `oCNpc::TurnToEnemy` @0x00737CD0, which is the **player-only** target-lock turn
  (gated by `s_bUseOldControls`, `zinput` focus, `s_bTargetLocked`) — not the AI
  combat-facing path.

So: the original AI combat-facing turn rate equals the plain guild `turn_speed`
(1x). There is no 2x in-combat AI turn anywhere in the binary.

## OpenGothic file:line

`/Users/admin/Downloads/opengothic/game/world/objects/npc.cpp:1462-1466`
(inside `Npc::implTurnToFai`, the combat-FAI auto-rotate used for every melee
combatant including the player):

```
auto bs = bodyStateMasked();
if(bs!=BS_HIT) {
  //NOTE: Troll rotates during the hit, but not very fast - seem to be regulat speed
  step *= 2.f; // faster in combat
}
```

## Divergence

OpenGothic doubles the per-frame turn step (`step *= 2.f`) for all combat-FAI
turning except while `BS_HIT`. The original AI turn-to-enemy/turn-to-vob path
(`oCNpc::Turning` / `oCNpc::EV_TurnToVob`) uses the unmodified guild `turn_speed`
(1x). The base rate elsewhere already matches the original exactly
(`rotateTo`: `step *= dt/1000`, and OG `turn_speed` is in deg/s == original
`turnSpeed_degPerMs * frameMs`), so this `*2` is a net +100% combat turn-rate
divergence: NPCs (and the player) snap to face their target roughly twice as fast
in melee as in vanilla Gothic II.

## Proposed patch (DEFERRED — see reason)

```
OLD:
  auto bs = bodyStateMasked();
  if(bs!=BS_HIT) {
    //NOTE: Troll rotates during the hit, but not very fast - seem to be regulat speed
    step *= 2.f; // faster in combat
    }

NEW:
  // NOTE: in original-game oCNpc::Turning @0x00683120 / oCNpc::EV_TurnToVob @0x00686160
  // the AI combat turn-to-enemy uses the plain guild turn_speed (1x). The only x4
  // multiplier is the player-only oCNpc::TurnToEnemy @0x00737CD0 (target-lock), not
  // this AI-facing path. No in-combat AI turn-speed doubling exists in Gothic2.exe.
  (void)bodyStateMasked();
```

**DEFERRED reason:** the rate factor itself is hard decompiled ground truth (1x,
not 2x), so this is not pure guesswork — but the existing `*= 2.f` is an explicit,
intentional OG gameplay-feel choice (comment "faster in combat"), and combat
turn-rate is precisely the kind of movement-feel value the parity rules say to
defer. Removing it would slow every melee NPC's facing to vanilla speed, which is
a noticeable feel change that should be validated at runtime against the original
before committing. Recommend keeping as a documented, runtime-verifiable candidate
rather than a blind build-time edit. (The `skipAnim = turn_speed >= 100` quirk on
line 1456 is a separate OG-specific heuristic and is intentionally left untouched.)
