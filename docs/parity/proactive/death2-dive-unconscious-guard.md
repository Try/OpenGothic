# Death-vs-Unconscious: diving NPC drops unconscious instead of dying (water guard misses Dive state)

**Confidence:** Medium-High. The code-level divergence is certain (verified against the
decompiler on both sides); real-world reachability is an edge case (a non-hostile human NPC
reduced to <=0 HP by weapon/spell damage while fully submerged/diving).

## Original function + address (prose only)

The death-vs-unconscious decision lives in `oCNpc::OnDamage_Condition` (Gothic2.exe `0x0066cf30`),
which sets two bits in the damage descriptor at byte offset `0x90`: bit `0x8` = "drop unconscious",
bit `0x4` = "die". `oCNpc::OnDamage_Events` (`0x0067abe0`) then dispatches: bit `0x8` calls
`oCNpc::DropUnconscious` (`0x00735eb0`), bit `0x4` calls `oCNpc::DoDie` (`0x00736760`).

Inside `OnDamage_Condition`, the unconscious branch is taken only when ALL hold:
victim is dead-ish or HITPOINTS==1, an attacker is present, the script predicate
(`C_DropUnconscious`) allows it, AND **`oCAniCtrl_Human::IsInWater(...)==0`** — i.e. the victim
must not be in water. If the unconscious bit was not set and the victim is dead, the death bit is
set instead.

`oCAniCtrl_Human::IsInWater` (`0x006b8a40`) returns true when the anictrl water-level field
(`+0x88`) is **1 (swimming) OR 2 (diving)** and the body is below the water surface. The sibling
`GetWaterLevel` (`0x006b89d0`) confirms the encoding: 1 = swim, 2 = dive, 0 = dry/wading. So the
original suppresses unconscious for BOTH swimming and diving victims — they die instead.

## OpenGothic file:line

`game/world/objects/npc.cpp:2119` (combat/spell damage entry `Npc::takeDamage`), which folds the
water guard into the `dontKill` flag passed down as `allowUnconscious`:

```cpp
const bool dontKill = ((b==nullptr && splId==0) || (bMask & COLL_DONTKILL)) && (!isSwim());
```

The guard only excludes `isSwim()`. In OpenGothic the `MoveAlgo` states `InWater`, `Swim`, `Dive`
are mutually exclusive (`game/game/movealgo.cpp:789-798`): `isSwim()` is true ONLY for the `Swim`
state, while `isDive()` is a separate state. The original's `IsInWater` (water-level 1 OR 2)
corresponds to OpenGothic `isSwim() || isDive()`.

## Divergence

A fully submerged (diving) NPC has `isSwim()==false`, so `dontKill` stays true. When such a
non-hostile human NPC is reduced to <=0 HP by a weapon hit or offensive spell, OpenGothic enters
the unconscious branch of `Npc::checkHealth` (npc.cpp:568-576), resetting HITPOINTS to 1 and
playing the unconscious state. The original engine, with `IsInWater` true for water-level 2,
skips the unconscious branch and sets the death bit — the NPC dies. (Swimming victims, water-level
1, are already handled correctly by OpenGothic's `!isSwim()`.)

Note this is the combat/spell path; the drown path is unaffected — `Npc::takeDrownDamage`
(npc.cpp:2241) already passes `allowUnconscious=false`.

## Proposed patch

```cpp
// OLD (game/world/objects/npc.cpp:2119)
const bool               dontKill   = ((b==nullptr && splId==0) || (bMask & COLL_DONTKILL)) && (!isSwim());

// NEW
// NOTE: in original-game oCNpc::OnDamage_Condition @0x0066cf30 the unconscious branch is gated on
// oCAniCtrl_Human::IsInWater(...)==0 (@0x006b8a40), which is true for water-level 1 (swim) AND
// water-level 2 (dive). OpenGothic's mutually-exclusive MoveAlgo states make isSwim() match only
// Swim, so a diving victim slipped past the guard and dropped unconscious instead of dying.
const bool               dontKill   = ((b==nullptr && splId==0) || (bMask & COLL_DONTKILL)) && (!isSwim() && !isDive());
```

Both `isSwim()` and `isDive()` are existing `Npc` members (declared npc.h:197-198, defined
npc.cpp:1068-1074); the shallow-wading `isInWater()` (MoveAlgo `InWater` == original water-level 0)
is correctly left out of the guard, matching the original.
