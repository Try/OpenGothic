# FightAI: enemy_stormprehit gated under isPrehit() makes anti-storm reaction unreachable

**Confidence:** High

## Original function + address

`oCNpc::FindNextFightAction` (Gothic2.exe `0x0067d680`, source `oNpc_Fight.cpp`)
builds the fight-move decision by walking a fixed priority list of situation
slots (a `do { switch(idx) ... } while(move==NOP)` loop). Each slot is an
*independent* priority band; the first one whose condition matches yields the
move table that is then randomly sampled (`GetFightActionFromTable`,
`0x0067ce30`).

The first two bands are:

- Band 0 -> `enemy_prehit` table. Condition (prose): the target can reach me
  (`IsInFightRange(target, this)`), the target has me in focus
  (`IsInFightFocus(target, this)`), I have the target inside my front cone
  (`|yaw| < 90` and `|pitch| < 50`), AND the target's current combat situation
  equals the "pre-hit" code `0xc`.
- Band 1 -> `enemy_stormprehit` table. Condition (prose): identical to band 0
  for range/focus/cone, EXCEPT the target's situation code must equal `0x10`.

The target's situation code is produced by a helper (`0x0067ce70`) that maps the
target's *currently active animation / body state* to one constant: `0xc` is
returned only on the pre-hit ani path (`IsInPreHit`), while `0x10` is returned
only on the "walking/running toward me" path (`IsWalking` + the run-forward
ani). These two paths return early and are mutually exclusive — a target is
either winding up a hit (`0xc`) or charging at me (`0x10`), never both at the
same instant. The storm-prehit reaction therefore fires when the enemy is
**charging/running** at the NPC (a storm attack uses BS_RUN), and it is NOT
predicated on the enemy being in a pre-hit animation.

## OpenGothic file:line

`game/game/fightalgo.cpp:49-55` (`FightAlgo::fillQueue`)

```cpp
if(tg.isPrehit() && isInWRange(tg,npc,owner) && isInFocusAngle(tg,npc) && isInFocusAngle(npc,tg,90)){
  if(tg.bodyStateMasked()==BS_RUN)
    if(fillQueue(owner,ai.enemy_stormprehit))
      return;
  if(fillQueue(owner,ai.enemy_prehit))
    return;
  }
```

## Divergence

OpenGothic collapses the original's two independent priority bands into one
block guarded by `tg.isPrehit()`, and only *inside* that guard does it check
`tg.bodyStateMasked()==BS_RUN` to pick `enemy_stormprehit`. That requires the
target to be in a pre-hit pose (`0xc`) AND in BS_RUN (`0x10`) simultaneously.

In the original these are mutually exclusive target situations: storm-prehit
(band 1) requires situation `0x10` (charging) and does **not** require the
pre-hit pose `0xc`. The OG `storm` attack itself is documented to run in BS_RUN
(see `npc.cpp:1602` "'storm' attack has BS_RUN state"), and while the attacker
is still charging its pose is not yet "prehit". As a result the inner
`tg.bodyStateMasked()==BS_RUN` branch is essentially never taken, and
`ai.enemy_stormprehit` is effectively **dead**: NPCs never play the dedicated
anti-storm reaction (e.g. jump-back/parry vs a charging foe) defined by the MDS
`enemy_stormprehit` table. They instead fall through to the generic
attack/focus bands, changing fight behaviour against charging opponents.

## Proposed patch

Make the storm-prehit and prehit reactions two independent checks keyed off the
target's situation, matching the original's two priority bands. The storm band
keys off the target charging (BS_RUN); the prehit band keys off `tg.isPrehit()`.
Both share the same range/focus/front-cone gate.

OLD (`game/game/fightalgo.cpp`):
```cpp
  if(tg.isPrehit() && isInWRange(tg,npc,owner) && isInFocusAngle(tg,npc) && isInFocusAngle(npc,tg,90)){
    if(tg.bodyStateMasked()==BS_RUN)
      if(fillQueue(owner,ai.enemy_stormprehit))
        return;
    if(fillQueue(owner,ai.enemy_prehit))
      return;
    }
```

NEW:
```cpp
  // NOTE: in original-game oCNpc::FindNextFightAction (Gothic2.exe 0x0067d680) the
  // enemy_stormprehit and enemy_prehit reactions are two independent priority bands.
  // Band 1 (storm) fires when the target's situation is "charging" (BS_RUN, situation
  // code 0x10) and does NOT require the target to be in a prehit pose; band 0 (prehit)
  // fires on the prehit situation (code 0xc). The two target states are mutually
  // exclusive, so nesting stormprehit under isPrehit() (as before) made the dedicated
  // anti-storm reaction unreachable. Both bands share the range/focus/front-cone gate.
  if(isInWRange(tg,npc,owner) && isInFocusAngle(tg,npc) && isInFocusAngle(npc,tg,90)){
    if(tg.bodyStateMasked()==BS_RUN)
      if(fillQueue(owner,ai.enemy_stormprehit))
        return;
    if(tg.isPrehit())
      if(fillQueue(owner,ai.enemy_prehit))
        return;
    }
```

Symbols verified present: `FightAlgo::isInWRange`, `FightAlgo::isInFocusAngle`
(both overloads), `Npc::isPrehit`, `Npc::bodyStateMasked`, `BS_RUN`,
`FightAi::FA::enemy_stormprehit`, `FightAi::FA::enemy_prehit`,
`FightAlgo::fillQueue(GameScript&, const zenkit::IFightAi&)`.

Note: the original additionally gates the front cone on pitch (`|pitch| < 50`),
which OpenGothic omits — that pitch component is tracked separately under the
already-filed `fai-prehit-react-angle` item and is intentionally not changed
here.
