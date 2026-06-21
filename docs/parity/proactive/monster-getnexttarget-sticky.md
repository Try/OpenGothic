# Monster target selection: Npc_GetNextTarget ignores the current enemy and always flips to the nearest foe

**Confidence:** High

## Original function + address

`oCNpc::GetNextEnemy` @ `0x00734e30` is the implementation of the Daedalus external
**`Npc_GetNextTarget`** (external dispatcher `FUN_006ecec0`, string `s_Npc_GetNextTarget`,
which calls `GetNextEnemy` on the resolved self). Field layout confirmed against
`oCNpc::SetEnemy` @ `0x00734bc0`:

- `this+0x498` = the NPC's current enemy pointer.
- `enemy+0x1b8` = enemy HP (validity test is `HP > 0`).
- `oCNpc_States::IsInState(enemy+0x588, -4 / -5)` = enemy is in a "down/flee" AI state.
- the enemy is also rejected if it is held by an incapacitating control spell
  (sleep `0x21`, plus `0x47/0x52/0x27/0x25/0x56` = freeze/petrify/charm-class).

Behavioral structure of the original:

1. **Sticky fast-path (top of function):** if the NPC already has a current enemy
   (`this+0x498 != 0`) and that enemy is still a *valid combatant* — `HP > 0`, not in
   AI-state `-4`/`-5`, and not incapacitated by the listed spells — the function
   **returns that same enemy immediately, without scanning**. The current target is
   retained.
2. Only when there is no valid current enemy does it iterate the NPC's nearby-vob list
   (`this+0x998`, count `this+0x9a0`), skip self / non-enemies / spell-incapacitated
   foes, pick the **minimum `GetDistanceToVob2` (squared distance)** candidate, and
   `SetEnemy` it.

So in the original a monster that has locked onto target A keeps returning A from every
`Npc_GetNextTarget` call in its attack loop (`ZS_MM_Attack` / `B_MM_*` / `B_AssessEnemy`),
even when a closer enemy B steps into range; it only re-acquires once A becomes invalid.

## OpenGothic file:line

`game/game/gamescript.cpp:2492` — `GameScript::npc_getnexttarget`.

## Divergence

OpenGothic's `npc_getnexttarget` has **no sticky fast-path**. Every call unconditionally
runs `world().detectNpc(...)` and selects the **closest** sensible enemy by squared
distance, then `npc->setTarget(ret)`:

```
world().detectNpc(npc->position(),float(npc->handle().senses_range),[&,npc](Npc& oth){
  if(&oth!=npc && !oth.isDown() && oth.isEnemy(*npc) && npc->canSenseNpc(oth,true)!=SensesBit::SENSE_NONE){
    float qd = oth.qDistTo(*npc);
    if(qd<dist){ dist=qd; ret = &oth; }
    }
  return false;
  });
if(ret!=nullptr)
  npc->setTarget(ret);
```

Consequence: a monster that is already fighting target A will **flip its target** to any
enemy B that happens to be closer on the next `Npc_GetNextTarget` tick, whereas the
original stays locked on A until A is dead / unconscious / fleeing. This visibly changes
pack and multi-foe encounters (e.g. monsters constantly switching between the player and
summoned/companion NPCs, or two players, based purely on who is momentarily nearest),
and it can cancel an in-progress attack via `setTarget`'s `clearGoTo()` side effect.

## Proposed patch

Add the sticky fast-path before the scan, returning the existing valid target unchanged.
Grep-verified OG symbols: `Npc::target()` (`npc.cpp:3110`), `Npc::isDown()`
(`npc.cpp:4188`, = unconscious || dead, the engine-side analogue of the original's
`HP>0`/down-state gate), `Npc::isEnemy`, `Npc::canSenseNpc`, `Npc::setTarget`,
`vm.global_other()`, `handlePtr()`.

The spell-incapacitation and exact `-4/-5` flee-state sub-checks are intentionally *not*
reproduced here (no grep-verified 1:1 engine equivalents for the control-spell list);
`isDown()` covers the dominant invalidation cases. This is the high-confidence,
common-case fix.

OLD (`game/game/gamescript.cpp`, `npc_getnexttarget`):
```cpp
bool GameScript::npc_getnexttarget(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  Npc* ret = nullptr;

  if(npc!=nullptr){
    float dist = float(npc->handle().senses_range);
    dist*=dist;
```

NEW:
```cpp
bool GameScript::npc_getnexttarget(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  Npc* ret = nullptr;

  if(npc!=nullptr){
    // NOTE: in original-game oCNpc::GetNextEnemy @0x00734e30 (Npc_GetNextTarget) keeps the
    // current enemy: if this->enemy is still a valid combatant it is returned without re-scanning,
    // so a monster does not flip to a merely-closer foe mid-fight. Only re-acquire when invalid.
    if(Npc* cur=npc->target();
       cur!=nullptr && cur!=npc && !cur->isDown() && cur->isEnemy(*npc) &&
       npc->canSenseNpc(*cur,true)!=SensesBit::SENSE_NONE) {
      auto s = vm.global_other();
      s->set_instance(cur->handlePtr());
      return true;
      }

    float dist = float(npc->handle().senses_range);
    dist*=dist;
```

(The remainder of the function — the `detectNpc` scan, `setTarget`, and the trailing
`global_other` write — is unchanged.)

NOTE citation to embed: `// NOTE: in original-game oCNpc::GetNextEnemy @0x00734e30 (Npc_GetNextTarget) ...`
