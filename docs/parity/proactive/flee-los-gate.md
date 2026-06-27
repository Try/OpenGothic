# Flee divergence: missing free-line-of-sight gate in `implAiFlee`

**Confidence:** Medium (real engine divergence; patch DEFERRED because net behavioral
impact inside OpenGothic's flee model is uncertain and a naive fix risks over-suppressing flee).

## Original function + address

The Daedalus `AI_Flee(self)` external in `Gothic2.exe` lives in `oGameExternal.cpp`
(decompiled as `FUN_006f5020`). It resolves `self`, then — gated on the NPC's event
manager — calls `oCNpc::Fleeing` at **0x006820c0** (the engine's actual flee-steering
routine; the adjacent `oCNpc::ThinkNextFleeAction` @0x006820d0 is the same body, and
`oCNpc::AI_Flee` @0x00683210 is an empty stub with no callers).

`oCNpc::Fleeing` reads the flee-from target from the NPC's *enemy* field (offset `+0x498`,
written by `oCNpc::SetEnemy`). Its very first substantive step, before computing any flee
direction or waypoint, is:

1. validate the enemy pointer is a live `zCVob` (RTTI walk), then
2. call `oCNpc::FreeLineOfSight(self, enemy)` (@0x... the `zCVob*` overload that probes the
   bbox-center of the enemy). **If there is no free line of sight to the enemy, `Fleeing`
   returns immediately and performs no flee steering.**

Only when LoS to the enemy is clear does it compute the away-direction
`(enemy - self) * -2.0` (constant `0xC0000000` = `-2.0`), project an escape point at roughly
twice the enemy distance behind the NPC, and query the waynet (`GetNearestWaypoint` /
`GetSecNearestWaypoint`) for a flee waypoint, falling back to a straight-line robust trace
(`RbtUpdate`, trace length `0x461C4000` = `10000.0`) when no waypoint is found. The net
intent: an NPC actively re-steers its flee only while it can see the threat, and stops
re-steering once it has broken line of sight (i.e. "escaped").

## OpenGothic file:line

`/Users/admin/Downloads/opengothic/game/world/objects/npc.cpp:1988` — `Npc::implAiFlee`.

## Divergence

`Npc::implAiFlee` guards only on `currentTarget==nullptr` (early return `true`) and
`isFalling()`. It then unconditionally scans waypoints within a fixed 5 m radius
(`maxDist = 5*100`) and turns toward the candidate farthest from the attacker, or directly
away from the attacker when no better waypoint exists. There is **no
`FreeLineOfSight`/`canSeeNpc(*currentTarget, ...)` gate** equivalent to the original's
`Fleeing` LoS check. OpenGothic therefore keeps re-steering the flee every tick regardless
of whether the fleeing NPC can still see the threat, whereas the original suppresses flee
steering once line of sight to the enemy is lost.

Two secondary, lower-confidence differences in the same routine (noted for context, not
proposed as fixes): the original searches for a flee waypoint at ~2x the enemy distance
behind the NPC rather than within a fixed 5 m radius, and the original does not reject
candidate waypoints by `useCounter()`/`underWater` the way OG's lambda does
(`npc.cpp:2001`, `2003`).

## Proposed patch

**DEFERRED.**

Reason: The divergence is real and grep-verified (OG has `Npc::canSeeNpc(const Npc&, bool)`
at `npc.h:386` / `npc.cpp:4821` and `currentTarget`, so a gate is mechanically expressible),
but I cannot reach high confidence that adding the gate is a net-positive parity fix without
in-game verification:

- The original's LoS gate operates inside a *stateful* flee machine (the `+0x4a0/+0x4a4`
  "has active flee target" flags and a persistent robust trace), so "no LoS -> return"
  there leaves a previously-committed flee trace running. OpenGothic's `implAiFlee` is
  stateless/recomputed-per-call and drives continuous motion through `GT_Flee` in
  `MoveAlgo`. Dropping a `return` in OG when LoS is lost could *freeze* a fleeing NPC mid
  open ground (no equivalent committed-trace fallback), which would be a worse divergence
  than the current always-flee behavior.
- Flee duration in OG is governed by the Daedalus side re-issuing `AI_Flee` (`ai_flee`
  external -> `aiPush(aiFlee)`), so the observable end-of-flee may already be script-driven
  rather than engine-driven; the engine LoS gate may be partially redundant.

A safe fix would need to preserve a "keep moving away along the last committed direction
when LoS is lost" fallback rather than a bare early-return, and be validated against a
fleeing-monster scenario in-game. Flagging for a follow-up with runtime verification.

<!-- NOTE: in original-game oCNpc::Fleeing @0x006820c0 the flee routine returns early
     (no re-steer) when FreeLineOfSight(self, enemy) is false; OpenGothic Npc::implAiFlee
     has no such line-of-sight gate on currentTarget. -->
