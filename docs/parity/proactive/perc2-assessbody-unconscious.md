# PERC_ASSESSBODY misses unconscious NPCs (body = dead-only instead of dead-or-unconscious)

**Confidence:** High

## Original function + address

`oCNpc::PerceptionCheck` (Gothic2.exe @ 0x0075dd30) is the active perception sweep. It
rebuilds the NPC's vob list (`CreateVobList` within `senses_range`) and classifies every
nearby vob into the perception slots player / enemy / fighter / **body** / item. A vob is
classified as a *body* candidate (the slot later dispatched to `AssessBody_S`, Gothic2.exe
@ 0x0075d4-area `case 4`) when, for that NPC, **`hp < 1` OR it is in hardcoded state -4**:

- `*(int*)(npc+0x1b8) < 1` is exactly `oCNpc::IsDead` (Gothic2.exe @ 0x00736740, returns
  `hp < 1`).
- `oCNpc_States::IsInState(npc.states, -4)` is exactly `oCNpc::IsUnconscious`
  (Gothic2.exe @ 0x00736750, which is literally `IsInState(states, -4)`, confirmed by
  decompiling `oCNpc_States::IsInState` @ 0x0076c040 and `IsUnconscious` @ 0x00736750).

So the original body predicate is `IsDead() || IsUnconscious()` — i.e. the union "down".
The candidate is then additionally gated by perception-range and `CanSense`, matching what
OpenGothic already does. The only divergence is the membership test.

## OpenGothic file:line

`game/world/objects/npc.cpp:2246` inside `Npc::updateNearestBody()` (the sole producer of
the `PERC_ASSESSBODY` candidate, consumed at `npc.cpp:4320-4326`):

```cpp
owner.detectNpcNear([this,&ret,&dist](Npc& n){
    if(!n.isDead())            // <-- dead-only
      return;
    ...
```

## Divergence

OpenGothic perceives a body only when the target `isDead()` (hp ≤ 0). The original game
also perceives a body when the target is **unconscious** (`IsInState(-4)`, hp still > 0 —
knocked-out NPCs keep positive HP). Consequently, after a non-lethal takedown / KO, nearby
NPCs that have `PERC_ASSESSBODY` (typical for guards) never run their `B_AssessBody`
reaction in OpenGothic, whereas in Gothic II they do. The same applies to an unconscious
player being found by NPCs. `updateNearestEnemy()` already correctly uses `isDown()`
(npc.cpp:2219/2225); only the body path regressed to `isDead()`.

## Proposed patch

`game/world/objects/npc.cpp`, `Npc::updateNearestBody()`:

OLD:
```cpp
  owner.detectNpcNear([this,&ret,&dist](Npc& n){
    if(!n.isDead())
      return;
```

NEW:
```cpp
  owner.detectNpcNear([this,&ret,&dist](Npc& n){
    // NOTE: in original-game oCNpc::PerceptionCheck @0x0075dd30 classifies a vob as a
    // "body" when IsDead() (hp<1) OR IsUnconscious() (oCNpc_States::IsInState(-4),
    // see oCNpc::IsUnconscious @0x00736750); unconscious NPCs keep hp>0, so a dead-only
    // test silently drops them as AssessBody candidates.
    if(!n.isDown())
      return;
```

`Npc::isDown()` is grep-verified to exist (npc.h:286, npc.cpp:4229-4231) and equals
`isUnconscious() || isDead()`, exactly the original union. Build-safe, surgical, one-token
behavioral change.
