# PERC_ASSESSTHREAT never raised when aiming a ranged weapon at an NPC

> DEFER: the cited npc.cpp:1705-1712 path is the AI archer's fight-action (per AI-tick), which would only cover NPC-vs-target, not the player-aiming-at-NPC case the finding describes (player aim is input-driven in playercontrol). It also adds a new per-tick reaction; needs the right dispatch site + runtime to confirm it doesn't spam.

**Confidence:** Medium

## Original behavior (Gothic2.exe)

`oCNpc::InterpolateAim(oCNpc* target)` @ 0x752cc0 is the per-frame ranged-aim
interpolation routine (called while an NPC or the player aims a bow/crossbow at
a target). Before computing the aim angles it unconditionally calls
`oCNpc::AssessThreat_S(target, this)` @ 0x75c060, i.e. it makes the *aimed-at*
NPC run its threat reaction with the aimer as OTHER.

`AssessThreat_S` (verified in the decompiler) does not broadcast or range-check:
when the target NPC has the THREAT perception registered it directly
`StartAIState(...)` for that perception's handler (`B_AssessThreat`). The perc id
is 10 (`PERC_ASSESSTHREAT`). Net gameplay effect: pointing a drawn bow/crossbow
at an NPC makes that NPC perceive a threat and react (turn to face / warn /
draw), exactly the classic "aim a bow at a guard and they respond" behavior.

## OpenGothic divergence

`PERC_ASSESSTHREAT` is dispatched **nowhere** in OpenGothic. A repo-wide search
finds the constant only in `game/game/constants.h:410` — it is never passed to
`sendPassivePerc` / `perceptionProcess`. The ranged-aim path
`game/world/objects/npc.cpp:1705-1712` (`shootBow()` / `aimBow()`) drives bow
aiming but never raises a threat perception on `currentTarget`. Therefore NPCs
in OpenGothic do not react at all to being aimed at with a ranged weapon, unlike
the original.

OG file:line — `game/world/objects/npc.cpp:1705-1712` (bow-aim branch);
`game/world/objects/npc.cpp:4084` (`aimBow`).

## Proposed patch

Raise the threat perception on the aim target when an NPC enters the bow-aim
branch with a valid `currentTarget`, mirroring `InterpolateAim`'s unconditional
`AssessThreat_S(target,this)`. Use the immediate/passive dispatch already used
for other passive percs.

File: `game/world/objects/npc.cpp`

OLD:
```cpp
    else if(ws==WeaponState::Bow || ws==WeaponState::CBow) {
      if(shootBow()) {
        fghAlgo.consumeAction();
        }
      else if(!implTurnToFai(*currentTarget,dt)) {
        aimBow();
        }
      }
```

NEW:
```cpp
    else if(ws==WeaponState::Bow || ws==WeaponState::CBow) {
      // NOTE: in original-game oCNpc::InterpolateAim (0x752cc0) calls
      // AssessThreat_S(target,this) every aim frame, so the aimed-at NPC runs
      // its PERC_ASSESSTHREAT (10) reaction. OpenGothic never raised it.
      owner.sendPassivePerc(*currentTarget,*this,*this,PERC_ASSESSTHREAT);
      if(shootBow()) {
        fghAlgo.consumeAction();
        }
      else if(!implTurnToFai(*currentTarget,dt)) {
        aimBow();
        }
      }
```

Note on signature: `WorldObjects::sendPassivePerc(Npc& self, Npc& other, Npc* victim, int32_t perc)`
queues a perc whose recipient is resolved by proximity to `self.position()`.
Here `self` is the target (recipient candidate) and `other`/`victim` is the
aimer, matching the original's `AssessThreat_S(target, aimer)` argument order
(SELF=target's perc owner, OTHER=aimer). If a self-only delivery is preferred,
`sendImmediatePerc` can be used instead; `sendPassivePerc` keeps it on the
normal passive-perc tick and respects `percRanges().at(PERC_ASSESSTHREAT,...)`.
