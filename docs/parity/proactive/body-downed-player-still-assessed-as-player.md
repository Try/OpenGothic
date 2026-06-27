# PERC_ASSESSPLAYER fires on a downed (dead/unconscious) player instead of routing to PERC_ASSESSBODY

**Confidence:** High

## Original function + address

`oCNpc::PerceptionCheck` @ `0x0075dd30` (the passive-perception driver that classifies every
vob in the perceiver's sense-radius vob list). For each NPC vob the function performs a single
**mutually-exclusive** classification: a vob is treated as a *body* candidate when its hitpoints
are below 1 (`hp < 1`) **or** when `oCNpc::IsUnconscious` (`0x00736750`, i.e.
`oCNpc_States::IsInState(-4)`) is true; only in that body branch is the body-assessment candidate
slot written and `AssessBody_S` later invoked. The *alive* branch (the `else` arm) is the only
place where the player / enemy / fighter candidate slots are written, so the player-assessment
state (`AssessPlayer_S`) can never be reached for a player whose hp is below 1 or who is
unconscious — a downed player is exclusively a body, never an "assessable player". (The companion
helper `oCNpc::IsInPerceptionRange` lives at `0x0075e460` / `0x0075e490`; the per-vob detection
gate `oCNpc::CanSense` at `0x00740740`.)

## OpenGothic file:line

`game/world/objects/npc.cpp:4450` (inside `Npc::perceptionProcess(Npc&)`).

## Divergence

OpenGothic already mirrors the body-classification *inclusion* fix in `updateNearestBody`
(npc.cpp:2309, gated by `isDown()`) and excludes downed NPCs from `updateNearestEnemy`
(npc.cpp:2296, `n.isDown()`), so PERC_ASSESSENEMY and PERC_ASSESSBODY are mutually exclusive for
ordinary NPCs.

But the PERC_ASSESSPLAYER branch has **no down-state gate**:

```cpp
const float quadDist = pl.qDistTo(*this);
if(hasPerc(PERC_ASSESSPLAYER) && canSenseNpc(pl,false)!=SensesBit::SENSE_NONE) {
  if(perceptionProcess(pl,nullptr,quadDist,PERC_ASSESSPLAYER)) {
    ret = true;
    }
  }
```

When the player is unconscious (knocked out by guards) or dead, the original routes the player
solely into the body branch (`AssessBody`). OpenGothic instead fires PERC_ASSESSPLAYER on the
downed player every perception tick (and *also* picks them up as a body via `updateNearestBody`,
since `npcNear`/`detectNpcNear` include the player). The result is that nearby NPCs run their
`B_AssessPlayer` reaction (greet / turn-to / aggro logic) against a corpse or a knocked-out hero,
where the original game only ever runs `B_AssessBody`. This is a player-facing behavioral split
distinct from the already-fixed `updateNearestBody` `isDown` inclusion and the
`updateNearestEnemy` down-exclusion.

## Proposed patch

Gate the player-assessment branch on the same `isDown()` predicate already used for the body /
enemy branches, so the dead-or-unconscious player is classified only as a body — matching the
original's mutually-exclusive `hp < 1 || IsUnconscious()` split. (`Npc::isDown()` exists at
npc.cpp:4375 and is `isUnconscious() || isDead()`, the codebase-consistent body predicate.)

OLD (`game/world/objects/npc.cpp:4450`):
```cpp
  if(hasPerc(PERC_ASSESSPLAYER) && canSenseNpc(pl,false)!=SensesBit::SENSE_NONE) {
```

NEW:
```cpp
  // NOTE: in original-game oCNpc::PerceptionCheck @0x0075dd30 a vob with hp<1 or
  // oCNpc::IsUnconscious @0x00736750 is classified exclusively as a body (AssessBody); the
  // player/enemy/fighter (AssessPlayer) branch is the else-arm, so a downed player is never
  // assessed via PERC_ASSESSPLAYER.
  if(hasPerc(PERC_ASSESSPLAYER) && !pl.isDown() && canSenseNpc(pl,false)!=SensesBit::SENSE_NONE) {
```
