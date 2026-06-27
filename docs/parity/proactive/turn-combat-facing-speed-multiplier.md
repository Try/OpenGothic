# Turn: combat-facing turn-speed multiplier (2x vs 4x)

**Confidence:** Medium

## Original function + address

In `Gothic2.exe` the AI melee fight loop (`oCNpc::FightMelee`, callsites at
`0x00696f7e` / `0x006978db`) keeps the attacker facing its target by calling
`oCNpc::TurnToEnemy` (`0x00737cd0`). That function computes the per-frame turn
step as:

  step = GetTurnSpeed(this) * <per-frame turn-time global @0x0099b3d8> * 4.0

The literal `4.0` is the immediate `0x40800000`. `oCNpc::GetTurnSpeed`
(`0x00680970`) returns the NPC's script-derived turn rate (field `+0x49c`,
i.e. the guild `turn_speed`, set in `oCNpc::SetScriptValues` @`0x006a5110` as
`scriptTurnSpeed * 0.001`), or a fixed value for the free-aim player case.

By contrast the *non-combat* AI turn messages — `oCNpc::EV_TurnToVob`
(`0x00686160`), `EV_Turn` (`0x00685de0`), `EV_TurnToPos` (`0x00686070`) and
`oCNpc::Turning` (`0x00683120`) — compute the step as
`turnSpeed * <per-frame turn-time global @0x0099b3ec>` with **no** extra
multiplier. So the original applies a 4.0x speed-up specifically on the melee
turn-to-enemy path, on top of the same base `turnSpeed * frameTime` used by the
ordinary turn path. `TurnToEnemy` is invoked from `FightMelee`, `MagicMode`,
`EV_Parade` and `DoAI`, i.e. the combat states.

## OpenGothic file:line

`/Users/admin/Downloads/opengothic/game/world/objects/npc.cpp:1474-1477`
(`Npc::implTurnToFai`, the in-combat "keep facing the focus enemy" turn):

```
auto bs = bodyStateMasked();
if(bs!=BS_HIT) {
  //NOTE: Troll rotates during the hit, but not very fast - seem to be regulat speed
  step *= 2.f; // faster in combat
  }
```

`step` here is `guildVal().turn_speed[gl]`, the same base rate the non-combat
`Npc::implTurnTo` uses with no multiplier (`npc.cpp:1504-1508`). So OpenGothic's
combat-facing turn is **2x** the base rate, while the original's
`TurnToEnemy` path is **4x** the base rate.

## Divergence

OpenGothic doubles the turn speed while facing the enemy in combat
(`step *= 2.f`); the original quadruples it (`* 4.0` in `TurnToEnemy`). The
overall structure matches (base path = 1x, combat path = multiplier), but the
constant differs, so OpenGothic NPCs rotate toward their target at half the
original's combat-turn rate. The in-source comment (`// faster in combat`,
and the nearby `// vanilla has a bug(or quirk) apparently`) shows the `2.f`
was an estimate rather than a measured value.

## Proposed patch

```
OLD:
  auto bs = bodyStateMasked();
  if(bs!=BS_HIT) {
    //NOTE: Troll rotates during the hit, but not very fast - seem to be regulat speed
    step *= 2.f; // faster in combat
    }

NEW:
  auto bs = bodyStateMasked();
  if(bs!=BS_HIT) {
    //NOTE: Troll rotates during the hit, but not very fast - seem to be regulat speed
    // NOTE: in original-game oCNpc::TurnToEnemy @0x00737cd0 (called from
    //       oCNpc::FightMelee) the combat turn step is GetTurnSpeed()*frameTime*4.0,
    //       whereas the non-combat oCNpc::EV_TurnToVob @0x00686160 path uses
    //       turnSpeed*frameTime with no multiplier => combat is 4x base, not 2x.
    step *= 4.f; // faster in combat
    }
```

Grep-verified OG symbols: `Npc::implTurnToFai`, `bodyStateMasked()`, `BS_HIT`,
local `step` (= `gv.turn_speed[gl]`) all exist at `npc.cpp:1452-1480`.

### Why Medium (not High)

The original's combat path reads its per-frame turn-time global from
`@0x0099b3d8` while the non-combat path reads `@0x0099b3ec` (two different
TU-local globals, in `oNpc.cpp` vs `oNpc_Move.cpp`). Both are read as the same
"per-frame turn scale" quantity in their respective translation units (each is
also used as `DAT/_ztimer` in the animation-driven turn paths
`oCAniCtrl_Human::Turn` @0x006ae540 and `FUN_00683000`), which strongly
suggests they hold the same value (current frame time) and therefore that the
combat path is a clean 4x of the base path. I could not *prove* `DAT_0099b3d8 ==
DAT_0099b3ec` because the warm decompiler does not index absolute-address data
references and the Ghidra MCP endpoint was unreachable during analysis. If the
two globals differ by a constant factor, the effective ratio would not be 4x.
The fix is build-verifiable and low-risk (one constant, combat-only path), but
the residual normalization assumption keeps this at Medium rather than High.
