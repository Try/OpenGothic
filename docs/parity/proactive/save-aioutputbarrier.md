# Save/Load bug: `aiOutputBarrier` not persisted (resets on reload)

**Confidence:** Medium

## The save/load asymmetry

`Npc::aiOutputBarrier` (`game/world/objects/npc.h:607`, `uint64_t aiOutputBarrier=0;`) is an
**absolute game-tick** timestamp that gates AI speech/output. It is mutated during gameplay in
`Npc::setAiOutputBarrier` but is **never written by `Npc::save()` / `saveAiState()` and never read
by `load()` / `loadAiState()`**. On reload it silently resets to `0`.

Mutation (`game/world/objects/npc.cpp:3129-3133`):
```cpp
void Npc::setAiOutputBarrier(uint64_t dt, bool overlay) {
  aiOutputBarrier = owner.tickCount()+dt;
  if(!overlay)
    outWaitTime = aiOutputBarrier;   // outWaitTime IS saved; aiOutputBarrier is NOT
  }
```

It gates real behavior:
- `haveOutput()` — `npc.cpp:3124` (`owner.tickCount()<aiOutputBarrier` → "busy")
- `performOutput()` — `npc.cpp:425` (holds back a non-player's own pending output)
- printScreen gating — `npc.cpp:2926`

The absolute timestamp stays meaningful across reload because `GameSession::ticks`
(`tickCount()`) is persisted and continues monotonically.

## Why `outWaitTime` does not cover it

For non-overlay output (`overlay==false`, e.g. dialog) the same value is also written to
`outWaitTime`, which **is** saved (`saveAiState` `npc.cpp:336`), so that path is mostly covered.
But the **overlay** path — `GameScript::aiOutput(... overlay=true)` →
`setAiOutputBarrier(dt, /*overlay*/true)` (`gamescript.cpp:1281`), reached via `AI_OutputSvmOverlay`
— sets **only** `aiOutputBarrier` and leaves `outWaitTime` untouched. An NPC saved mid SVM-overlay
output therefore loses its output barrier entirely on reload.

## Consequence

An NPC that was saved while an SVM-overlay line was in flight will, after load, report
`haveOutput()==false` from the barrier and no longer suppress its own queued output — its
voice-line/output timing barrier is lost, allowing premature/overlapping AI output for that NPC.

## Proposed patch

Persist `aiOutputBarrier` next to `outWaitTime` in the AI-state block, guarded behind a new save
version (bump `Serialize::Current` 55 → 56).

File: `game/world/objects/npc.cpp` — `saveAiState`

OLD:
```cpp
void Npc::saveAiState(Serialize& fout) const {
  fout.write(aniWaitTime,waitTime,faiWaitTime,outWaitTime);
```
NEW:
```cpp
void Npc::saveAiState(Serialize& fout) const {
  fout.write(aniWaitTime,waitTime,faiWaitTime,outWaitTime);
  fout.write(aiOutputBarrier);
```

File: `game/world/objects/npc.cpp` — `loadAiState`

OLD:
```cpp
void Npc::loadAiState(Serialize& fin) {
  fin.read(aniWaitTime);
  fin.read(waitTime,faiWaitTime);
  fin.read(outWaitTime);
```
NEW:
```cpp
void Npc::loadAiState(Serialize& fin) {
  fin.read(aniWaitTime);
  fin.read(waitTime,faiWaitTime);
  fin.read(outWaitTime);
  if(fin.version()>55)
    fin.read(aiOutputBarrier);
```

File: `game/game/serialize.h`

OLD:
```cpp
      Current    = 55,
```
NEW:
```cpp
      Current    = 56,
```
