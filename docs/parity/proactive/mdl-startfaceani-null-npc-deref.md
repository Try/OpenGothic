# Mdl_StartFaceAni: guards the script handle instead of the resolved NPC (null-deref divergence)

**Confidence:** Medium

## Original function + address

`Mdl_StartFaceAni` external — `Gothic2.exe` `FUN_006fae20` (string `Mdl_StartFaceAni` @ `0x008b48ac`).

The external reads its parameters (`holdTime`, `intensity`, face-ani name) and then resolves
the target NPC through the shared helper `FUN_006db090` (the "GetNpc-from-parameter" routine, the
same helper used by every other `Mdl_*` external such as `Mdl_ApplyOverlayMds` @ `FUN_006f9d40`).
The original then branches on the **resolved oCNpc pointer**: when it is null it emits the
`"... -> ani: ..."`/script-error report through `zERROR::Report` and returns without touching the
model; only when the pointer is non-null does it call `oCNpc::StartFaceAni(npc, name, intensity,
holdTime)` (`oCNpc::StartFaceAni` @ `0x00738860`). In other words the original never dereferences a
null NPC — a missing/unresolvable instance is a graceful no-op.

## OpenGothic file:line

`game/game/gamescript.cpp:2018-2021` (`GameScript::mdl_startfaceani`).

Helper `GameScript::findNpc(zenkit::INpc*)` is at `game/game/gamescript.cpp:1537-1542`; it returns
`nullptr` when the handle is null and otherwise `reinterpret_cast<Npc*>(handle->user_ptr)` — and
`user_ptr` can be null (the code itself flags this with `assert(handle->user_ptr); // engine bug, if
null`, which is compiled out in release builds).

## Divergence

```cpp
void GameScript::mdl_startfaceani(std::shared_ptr<zenkit::INpc> npcRef, std::string_view ani, float intensity, float time) {
  if(npcRef!=nullptr)
    findNpc(npcRef.get())->startFaceAnim(ani,intensity,uint64_t(time*1000.f));
  }
```

The guard tests the *script handle* `npcRef`, but the call dereferences the *resolved* pointer
`findNpc(npcRef.get())`. When the handle is non-null yet `findNpc` returns `nullptr` (handle with a
null `user_ptr` — the very "engine bug" case the assert documents, which silently returns `nullptr`
in release), OpenGothic dereferences null and crashes, whereas the original reports a script error
and returns harmlessly. Every other `Mdl_*` external in this file follows the original's pattern of
guarding the **resolved** npc (`auto npc = findNpc(npcRef); if(npc==nullptr) return;`), so this one
external is the outlier.

The identical defect exists at `game/game/gamescript.cpp:2012-2016`
(`GameScript::mdl_setmodelscale`, the `Mdl_SetModelScale` external): it resolves
`auto npc = findNpc(npcRef);` but then guards `if(npcRef!=nullptr)` before calling
`npc->setScale(...)`, so a null `npc` from a non-null handle is dereferenced.

## Proposed patch

Guard the resolved NPC, matching the original's "null oCNpc -> no-op" behaviour and the convention
used by the rest of the `Mdl_*` externals. Symbols grep-verified: `findNpc(const
std::shared_ptr<zenkit::INpc>&)` (`gamescript.cpp:1544`), `Npc::startFaceAnim`
(`npc.cpp:1033`), `Npc::setScale` (`npc.cpp:979`).

`mdl_startfaceani` — OLD:
```cpp
void GameScript::mdl_startfaceani(std::shared_ptr<zenkit::INpc> npcRef, std::string_view ani, float intensity, float time) {
  if(npcRef!=nullptr)
    findNpc(npcRef.get())->startFaceAnim(ani,intensity,uint64_t(time*1000.f));
  }
```

NEW:
```cpp
void GameScript::mdl_startfaceani(std::shared_ptr<zenkit::INpc> npcRef, std::string_view ani, float intensity, float time) {
  // NOTE: in original-game Mdl_StartFaceAni (Gothic2.exe FUN_006fae20) the external resolves the
  // npc first and skips (script-error, no-op) when it is null, never dereferencing it; guard the
  // resolved npc, not the script handle.
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    npc->startFaceAnim(ani,intensity,uint64_t(time*1000.f));
  }
```

`mdl_setmodelscale` — OLD:
```cpp
  auto npc = findNpc(npcRef);
  if(npcRef!=nullptr)
    npc->setScale(x,y,z);
```

NEW:
```cpp
  // NOTE: in original-game Mdl_SetModelScale the external no-ops on a null resolved npc.
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    npc->setScale(x,y,z);
```
