# Npc_GetNextTarget re-acquires nearest enemy every call instead of keeping the current valid foe (sticky target)

**Confidence:** High (divergence is unambiguous in both binaries; gameplay-severity medium-high)

## Original function + address

`oCNpc::GetNextEnemy` @ `0x00734e30` — the C++ handler behind the Daedalus
external `Npc_GetNextTarget` (registered/invoked from `FUN_006ecec0`, which
calls `oCNpc::GetNextEnemy(self)` and writes the result into the script
`OTHER` instance).

Behavior of the original function, in prose:

1. It first inspects the NPC's **current enemy** (the `enemy` pointer at offset
   `+0x498`, the same field read by the `Npc_GetTarget` handler `FUN_006ecd20`
   and written by `oCNpc::SetEnemy` @ `0x00734bc0`). If that current enemy is
   non-null, **alive** (`hitpoints > 0`), **not unconscious** (`oCNpc_States::IsInState(-4)`
   is false) and not in the terminal state (`IsInState(-5)` is false), and is
   not under a small set of disabling spells, the function **returns that same
   current enemy immediately, without scanning** for anything closer. The target
   is *sticky*.
2. Only when there is no valid current enemy does it scan its candidate NPC
   list, pick the nearest hostile that is alive/conscious, and commit it via
   `SetEnemy(best)` (where `best` may be null, which clears the enemy).

So the original semantics are: *"keep fighting the foe you already have as long
as it is alive and conscious; only re-acquire a target when you have none or it
became invalid."*

## OpenGothic file:line

`game/game/gamescript.cpp:2575` — `GameScript::npc_getnexttarget(...)`

## Divergence

OpenGothic's `npc_getnexttarget` **never consults the existing target**. Every
call it runs `world().detectNpc(...)` over `senses_range`, finds the nearest
enemy that passes `isEnemy`/`!isDown`/`canSenseNpc`, and `setTarget(ret)`s it.
Consequently:

- If the NPC is already fighting enemy A and a closer enemy B comes into range,
  OpenGothic switches the target to B; the original keeps A. When this external
  is polled inside a fight loop (the common `if(Npc_GetNextTarget(self)) ...`
  pattern), OpenGothic NPCs visibly thrash between attackers in a multi-enemy
  brawl, whereas the original commits to one foe until it dies or goes down.

`currentTarget` in OpenGothic is the exact analogue of the original `enemy`
field `+0x498` (`Npc::setTarget`/`Npc::target()` at `game/world/objects/npc.cpp:3186`
/`3195`), so the comparison is apples-to-apples.

## Proposed patch

Add the sticky early-out before scanning, matching the original's "valid current
enemy wins" path. `Npc::target()`, `Npc::isDown()` (`= isUnconscious() || isDead()`,
matching the original `hp>0 && !IsInState(-4) && !IsInState(-5)` gate),
`Npc::setTarget()`, `World::detectNpc`, `Npc::handle().senses_range`,
`Npc::isEnemy`, `Npc::canSenseNpc` are all grep-verified to exist.

OLD (`game/game/gamescript.cpp:2575`):
```cpp
bool GameScript::npc_getnexttarget(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  Npc* ret = nullptr;

  if(npc!=nullptr){
    float dist = float(npc->handle().senses_range);
    dist*=dist;

    world().detectNpc(npc->position(),float(npc->handle().senses_range),[&,npc](Npc& oth){
      if(&oth!=npc && !oth.isDown() && oth.isEnemy(*npc) && npc->canSenseNpc(oth,true)!=SensesBit::SENSE_NONE){
        float qd = oth.qDistTo(*npc);
        if(qd<dist){
          dist=qd;
          ret = &oth;
          }
        }
      return false;
      });
    if(ret!=nullptr)
      npc->setTarget(ret);
    }
```

NEW:
```cpp
bool GameScript::npc_getnexttarget(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  Npc* ret = nullptr;

  if(npc!=nullptr){
    // NOTE: in original-game oCNpc::GetNextEnemy @0x00734e30 (the Npc_GetNextTarget
    // handler) returns the *current* enemy unchanged when it is still alive and
    // conscious, instead of re-acquiring the nearest foe on every call (sticky
    // target). Only when there is no valid current enemy does it scan and re-commit
    // via SetEnemy (which also clears the enemy when nothing is found).
    Npc* cur = npc->target();
    if(cur!=nullptr && !cur->isDown()) {
      ret = cur;
      } else {
      float dist = float(npc->handle().senses_range);
      dist*=dist;

      world().detectNpc(npc->position(),float(npc->handle().senses_range),[&,npc](Npc& oth){
        if(&oth!=npc && !oth.isDown() && oth.isEnemy(*npc) && npc->canSenseNpc(oth,true)!=SensesBit::SENSE_NONE){
          float qd = oth.qDistTo(*npc);
          if(qd<dist){
            dist=qd;
            ret = &oth;
            }
          }
        return false;
        });
      npc->setTarget(ret);
      }
    }
```

Notes / scope:
- The original sticky gate additionally excludes a current enemy that is under a
  few disabling spells (IDs 0x21/0x47/0x52/0x27/0x25/0x56). OpenGothic does not
  model those spell effects on target validity, so they are intentionally omitted;
  `!isDown()` reproduces the alive+conscious portion of the gate.
- The `else` branch now calls `setTarget(ret)` unconditionally (rather than only
  when `ret!=nullptr`), matching the original scan path that always calls
  `SetEnemy(best)` and thereby **clears** the enemy when no valid target remains.
  This is the original's behavior; if a more conservative change is preferred,
  the clear-on-none could be kept as `if(ret!=nullptr) setTarget(ret)`, leaving
  only the sticky early-out as the fix.
