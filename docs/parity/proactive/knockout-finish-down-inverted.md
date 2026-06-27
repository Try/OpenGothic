# Knockout parity: inverted `death` flag when re-hitting a downed NPC

**Confidence:** Medium (high on the polarity bug; the exact faithful expression is reconstructed from the original's multi-stage damage chain, not a single function).

## Original function + address (prose only)

The original routes every hit through `oCNpc::OnDamage` (Gothic2.exe @0x006660e0). It does *not*
early-out when the victim is already down: it applies the damage, then calls
`oCNpc::OnDamage_Condition` (@0x0066cf30), then `oCNpc::OnDamage_Events` (@0x0067b257).

`OnDamage_Condition` decides the outcome with two independent result bits on the damage
descriptor:

- The **"victim becomes unconscious"** bit is set only when the hit is non-lethal-eligible:
  `(victim is effectively dead OR post-damage HP == 1)` AND an attacker exists AND the victim is
  **not in water** AND the script function `C_DropUnconscious` returns true. (`C_DropUnconscious`
  is the daedalus gate that returns true for fist / blunt / friendly-knockout hits and false for
  lethal weapon/arrow/spell hits.)
- The **"death"** bit is set only when the unconscious bit is *not* set and the victim is dead.

For an already-unconscious victim this yields the intuitive behaviour:
- hit again with a non-lethal blow (fists) → unconscious bit → `oCNpc::DropUnconscious`
  (@0x00735eb0) is re-invoked but **immediately returns** because the victim is already in AI
  state `-4` (`IsInState(-4)`), so the NPC simply **stays down**;
- hit again with a lethal weapon/arrow/spell → death bit → the NPC **dies (is finished)**.

For an already-dead victim, neither `C_DropUnconscious` nor the unconscious path applies, so it
**stays dead** regardless of hit type. Net truth table (already-down victim):

| current state | non-lethal hit (fists) | lethal hit (weapon/ranged/spell) |
|---|---|---|
| unconscious | stays unconscious | dies (finished) |
| dead | stays dead | stays dead |

## OpenGothic file:line

`game/world/objects/npc.cpp:2165` (inside `Npc::takeDamage`):

```cpp
if(isDown()) {
  onNoHealth(dontKill,HS_NoSound);
  return;
  }
```

`dontKill` is OpenGothic's "this hit can knock unconscious instead of kill" flag (defined at
npc.cpp:2135 and used as the `allowUnconscious` argument at npc.cpp:2208). `onNoHealth(bool death,
…)` (npc.cpp:584) drives the victim to `ZS_Dead` when `death==true` and to `ZS_Unconscious`
(setting HP back to 1) when `death==false`.

## Divergence

OpenGothic passes the non-lethal flag `dontKill` **directly** as the `death` argument, which is
polarity-inverted, and it also ignores the victim's current down-state. Resulting behaviour:

| current state | non-lethal hit (`dontKill==true`) | lethal hit (`dontKill==false`) |
|---|---|---|
| unconscious | `death=true` → **dies** (wrong: should stay down) | `death=false` → **stays unconscious** (wrong: should be finished) |
| dead | `death=true` → stays dead (ok) | `death=false` → **revives to unconscious, HP=1** (wrong) |

So OpenGothic kills a knocked-out NPC when you keep punching it (the classic "you can't kill a
downed NPC with fists" rule is broken), fails to let a lethal weapon finish a downed NPC, and can
even resurrect a corpse hit by a stray arrow/spell. Three of the four cases are wrong; only
"corpse + melee" happens to land correctly.

## Proposed patch

`death` for a downed victim should be `true` when the victim is already dead (re-assert dead) OR
when the new hit is lethal (`!dontKill`), reproducing the original truth table exactly.

```cpp
// OLD
if(isDown()) {
  onNoHealth(dontKill,HS_NoSound);
  return;
  }

// NEW
if(isDown()) {
  // NOTE: in original-game oCNpc::OnDamage_Condition @0x0066cf30 a re-hit on a downed NPC only
  // transitions to ZS_Dead when the victim is already dead or the blow is lethal (C_DropUnconscious
  // == false); a non-lethal blow (fists) leaves an unconscious victim down (oCNpc::DropUnconscious
  // @0x00735eb0 early-returns on IsInState(-4)). 'dontKill' is OpenGothic's non-lethal flag, so the
  // raw 'death=dontKill' here was inverted: it killed knocked-out NPCs with fists and could revive
  // corpses with ranged/spell hits.
  onNoHealth(isDead() || !dontKill,HS_NoSound);
  return;
  }
```

`isDead()` is grep-verified (`game/world/objects/npc.h:283`, defined npc.cpp:4303); `isDown()`,
`onNoHealth`, and `dontKill` are the existing symbols already on these lines. Build-verifiable,
single-statement change.
