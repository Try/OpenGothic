# Bind `Npc_IsNextTargetAvailable`

**Confidence:** High

## Original function + address

The Daedalus external `Npc_IsNextTargetAvailable` is handled by the function at
**Gothic2.exe `0x006ed090`** (`P:\dev\g2addon\release\Gothic\_ulf\oGameExternal.cpp`).
Its entire body is: pop the npc argument, and if non-null call
`oCNpc::GetNextEnemy` (**`0x00734e30`**), then `SetReturn(result != null)`. It does
**not** touch the script `OTHER` global.

This is the same `oCNpc::GetNextEnemy` used by the already-ported
`Npc_GetNextTarget` handler (**`0x006ecec0`**). The *only* difference between the two
externals is that `Npc_GetNextTarget` also writes the found enemy into the `OTHER`
instance (`zCParser::SetInstance`), whereas `Npc_IsNextTargetAvailable` returns the
boolean alone.

`oCNpc::GetNextEnemy` is "sticky": it returns the current enemy unchanged when that
enemy is still alive and not in state `-4`/`-5`; otherwise it scans for the nearest
sensible foe and commits it via `SetEnemy` (committing `null` when nothing is found),
returning that enemy. OpenGothic already reimplements exactly this logic, with a
detailed parity note, inside `GameScript::npc_getnexttarget`.

## OpenGothic file:line

- Unbound: there is no `bindExternal("npc_isnexttargetavailable", ...)` anywhere in
  `game/game/gamescript.cpp` or `game/gothic.cpp` (verified by grep, count 0). When a
  script calls it, the engine returns the default `0`/false, so combat AI that gates
  on `Npc_IsNextTargetAvailable` never sees an available target.
- Existing sibling logic to mirror: `game/game/gamescript.cpp:2645`
  (`GameScript::npc_getnexttarget`), bound at `game/game/gamescript.cpp:214`, declared
  at `game/game/gamescript.h:338`.

## Divergence

`Npc_IsNextTargetAvailable` is missing entirely. The original returns
`GetNextEnemy(npc) != null` (with the sticky re-acquire + `SetEnemy` side effect) but
without setting `OTHER`. OpenGothic currently returns false unconditionally.

All required building blocks already exist and are grep-verified:
`Npc::target()` / `Npc::setTarget(Npc*)` (`npc.h:397-398`), `Npc::isDown()`
(`npc.h:286`), `Npc::isEnemy(const Npc&)` (`npc.h:282`), `Npc::canSenseNpc(...)`
(`npc.h:394`), `Npc::qDistTo(const Npc&)` (`npc.h:133`), `World::detectNpc(...)`,
`SensesBit::SENSE_NONE`, and `npc->handle().senses_range` — all used verbatim by the
existing `npc_getnexttarget`.

## Proposed patch

Mirror `npc_getnexttarget`'s already-verified `GetNextEnemy` reimplementation, but
return only the boolean and do **not** write the `OTHER` global.

### 1. `game/game/gamescript.cpp` — register the external (after line 214)

OLD:
```cpp
  bindExternal("npc_getnexttarget",              &GameScript::npc_getnexttarget);
```
NEW:
```cpp
  bindExternal("npc_getnexttarget",              &GameScript::npc_getnexttarget);
  bindExternal("npc_isnexttargetavailable",      &GameScript::npc_isnexttargetavailable);
```

### 2. `game/game/gamescript.h` — declare (after line 338)

OLD:
```cpp
    bool npc_getnexttarget   (std::shared_ptr<zenkit::INpc> npcRef);
```
NEW:
```cpp
    bool npc_getnexttarget   (std::shared_ptr<zenkit::INpc> npcRef);
    bool npc_isnexttargetavailable(std::shared_ptr<zenkit::INpc> npcRef);
```

### 3. `game/game/gamescript.cpp` — implementation (insert after the end of
`npc_getnexttarget`, before `npc_sendpassiveperc` at line 2693)

NEW:
```cpp
bool GameScript::npc_isnexttargetavailable(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  if(npc==nullptr)
    return false;

  // NOTE: in original-game Npc_IsNextTargetAvailable (Gothic2.exe 0x006ed090) -> oCNpc::GetNextEnemy
  // @0x00734e30 returns the *current* enemy unchanged when it is still alive (not in state -4/-5),
  // otherwise re-acquires the nearest sensible foe and commits it via SetEnemy; the external returns
  // whether such an enemy exists. Unlike Npc_GetNextTarget @0x006ecec0 it does NOT write the script
  // `other` global. Same sticky logic as npc_getnexttarget above (kept in sync).
  Npc* ret = npc->target();
  if(ret==nullptr || ret->isDown()) {
    float dist = float(npc->handle().senses_range);
    dist*=dist;
    ret = nullptr;
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

  return ret!=nullptr;
  }
```

### Notes / risk

- Behavior is a strict subset of the already-shipped `npc_getnexttarget` (same
  `GetNextEnemy` reimplementation), minus the `global_other()->set_instance(...)`
  side effect — matching the original handler's omission of `SetInstance(OTHER, ...)`.
- The scan logic is duplicated rather than refactored into a shared helper to avoid
  any regression risk to the carefully-noted `npc_getnexttarget`. If a follow-up wants
  DRY, extract a private `Npc* findNextEnemy(Npc&)` and have both call it.
- Build-verifiable: every symbol used is grep-confirmed in `npc.h`/`world.h` and is
  already exercised by neighbouring code.
