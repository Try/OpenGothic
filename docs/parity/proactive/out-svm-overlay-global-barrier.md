# AI_OutputSVM_Overlay: global SVM barrier drops concurrent overlay voices from other NPCs

**Confidence:** Medium-High (divergence is certain; the proposed surgical fix is an approximation that is strictly closer to the original than the current code).

## Original function + address

`oCNpc::EV_OutputSVM_Overlay` (Gothic2.exe @ `0x00756a60`) is the message handler that
backs the `AI_OutputSVM_Overlay` external. Its gating works as follows (described, not quoted):

1. It resolves the SVM speech string to an *output-unit* index via
   `oCSVMManager::GetOU` (@ `0x00779e50`), using the speaker's voice id
   (the `+0x254` field, the NPC voice).
2. It then asks the cutscene manager whether the *specific SVM module* that owns
   that output unit is currently running, via the virtual
   `zCCSManager::LibIsSvmModuleRunning` (@ `0x00419e80`) (vtable slot `+0x5c`,
   reached after `LibGetSvmModuleName` at slot `+0x54`).
3. **Only if that one module is already running** does it bail out early
   (it emits the `B: AI: SVM Module is still running` report and returns
   without starting a new cutscene). Otherwise it starts the module / cutscene
   and plays the line.

The crucial property: the drop decision is keyed **per SVM module**, which is
derived from the speaking NPC's voice + the requested output unit. Two different
NPCs (different voices) almost always map to different modules and therefore run
their overlay SVMs independently and concurrently. The non-overlay
`oCNpc::EV_OutputSVM` (@ `0x007571f0`) has no such "already running" check at all.

## OpenGothic file:line

- `game/game/gamescript.cpp:1286` — `GameScript::aiOutputSvm`
- `game/game/gamescript.h:466` — `uint64_t svmBarrier = 0;` (single, GameScript-global)

```cpp
bool GameScript::aiOutputSvm(Npc &npc, std::string_view outputname, bool overlay) {
  if(overlay) {
    if(tickCount()<svmBarrier)
      return true;
    svmBarrier = tickCount()+messageTime(outputname);
    }
  if(!outputname.empty())
    return aiOutput(npc,outputname,overlay);
  return true;
  }
```

The overlay path (reached via `GlobalOutput::outputOv` → `aiOutputSvm(..., true)`,
`gamescript.cpp:61`) is gated by a **single process-global** timestamp
`svmBarrier` on `GameScript`, shared across every NPC.

## Divergence

In the original, an overlay SVM is dropped only when *that NPC's own* SVM module
is still running. In OpenGothic, while any one NPC's overlay SVM is within its
`messageTime` window, the global `svmBarrier` silently swallows the overlay SVM
of **every other NPC** as well (`tickCount()<svmBarrier` returns `true` =
"handled, drop"). In crowded ambient scenes where several NPCs fire
`AI_OutputSVM_Overlay` near-simultaneously (e.g. multiple guards/citizens
reacting), the original plays them as independent voice lines; OpenGothic plays
only the first and discards the rest for the duration of that line. The barrier
should be per-speaker, not global.

A secondary nit: `messageTime(outputname)` is called (and assigned into the
barrier) even when `outputname` is empty (SVM lookup miss); in the original a
failed `GetOU` starts nothing and arms no barrier. This is benign in practice
(`messageTime("")` resolves to `0`) but is extra work on the miss path.

## Proposed patch

Move the overlay barrier from a single GameScript-global field to per-NPC, so
that one speaker's overlay no longer blocks others. This matches the original's
per-module keying far more closely than a global gate (same-NPC repeats still
collide, which is the common case the original also rejects; cross-NPC concurrency
is restored). All referenced symbols are grep-verified to exist:
`Npc::aiOutputBarrier` (`game/world/objects/npc.h:607`), `Npc::setAiOutputBarrier`
(`npc.cpp:3144`), `Npc::owner` / `owner.tickCount()` (used at `npc.cpp:428`).

A minimal, surgical option re-uses the already-persisted per-NPC overlay channel
by adding a dedicated per-NPC `svmBarrier` field (kept distinct from
`aiOutputBarrier`, which serves the output-unit wait at `npc.cpp:428`).

OLD (`game/game/gamescript.cpp:1286`):
```cpp
bool GameScript::aiOutputSvm(Npc &npc, std::string_view outputname, bool overlay) {
  if(overlay) {
    if(tickCount()<svmBarrier)
      return true;
    svmBarrier = tickCount()+messageTime(outputname);
    }

  if(!outputname.empty())
    return aiOutput(npc,outputname,overlay);
  return true;
  }
```

NEW:
```cpp
bool GameScript::aiOutputSvm(Npc &npc, std::string_view outputname, bool overlay) {
  // NOTE: in original-game oCNpc::EV_OutputSVM_Overlay @0x00756a60 the overlay is
  // dropped only when *this speaker's* SVM module is still running
  // (zCCSManager::LibIsSvmModuleRunning @0x00419e80), keyed per NPC voice/module
  // via oCSVMManager::GetOU @0x00779e50 — not by a process-global timer. Use a
  // per-NPC barrier so a concurrent overlay from another NPC is not swallowed.
  if(overlay) {
    if(npc.aiOutputSvmBarrier()>tickCount())
      return true;
    if(!outputname.empty())
      npc.setAiOutputSvmBarrier(tickCount()+messageTime(outputname));
    }

  if(!outputname.empty())
    return aiOutput(npc,outputname,overlay);
  return true;
  }
```

with a new per-NPC `uint64_t svmBarrier=0;` field on `Npc` plus trivial
`aiOutputSvmBarrier()`/`setAiOutputSvmBarrier()` accessors, and removal of the
now-unused `GameScript::svmBarrier` (`gamescript.h:466`). The new field should be
serialized alongside the existing v56 `aiOutputBarrier` write/read
(`npc.cpp:337`/`356`) under a new save version to preserve mid-line state across
reload, mirroring the existing barrier handling.

**Residual gap (why not full-confidence):** the original keys on the resolved SVM
*module name*, so two different lines from the same NPC mapping to different
modules could in principle overlap, and the "still running" state is the cutscene
runtime, not a fixed `messageTime` estimate. A faithful port would resolve the
module per call and track its running state. The per-NPC barrier above does not
reproduce that exactly, but it removes the clearly-wrong cross-NPC suppression and
is strictly more correct than the current global gate. If a per-NPC field +
save-version bump is judged too invasive for a "surgical" change, treat this as
**DEFERRED** pending the module-name-keyed reimplementation.
