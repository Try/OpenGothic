# Bind unbound external `Npc_IsNear` (fixed 500-unit proximity test)

**Confidence:** High

## Original function + address

`Npc_IsNear` is registered by `DefineExternals_Ulfi` in `Gothic2.exe`; its external
handler is `FUN_006f1f70` (oGameExternal.cpp). The handler pops two `C_NPC` instance
arguments, and if both resolve to live NPCs it returns `oCNpc::IsNear` (member at
`0x0073fcc0`); otherwise it returns 0/FALSE.

`oCNpc::IsNear(other)` computes the squared 3D distance between the two NPCs' world
positions (the translation components of their vob transforms, at offsets 0x48/0x58/0x68)
and returns TRUE when that squared distance is strictly less than the constant
`0x48742400`, which decodes to the float `250000.0`. That threshold is a squared
distance, so the effective radius is `sqrt(250000) == 500` engine units. The test is
symmetric in its two operands, so argument order is irrelevant to the result.

In short: `Npc_IsNear(a, b)` returns TRUE iff `qDist(a,b) < 250000` (i.e. within 500
units), and FALSE if either NPC handle is invalid.

## OpenGothic file:line

- Binding block: `game/game/gamescript.cpp:182` (sits next to `npc_getdisttonpc`); the
  external is absent from the entire `bindExternal(...)` block (`gamescript.cpp:111-327`).
- Verified unbound: no `npc_isnear` / `IsNear` token anywhere under `game/`. The VM routes
  it to the default not-implemented handler registered at `game/gothic.cpp:964`.

## Divergence

`Npc_IsNear` is never bound, so any script call falls through to
`notImplementedRoutine` and yields the default 0/FALSE return, regardless of the actual
distance between the two NPCs. The original returns TRUE for NPCs within 500 units.

## Proposed patch

OpenGothic already exposes the exact building block: `Npc::qDistTo(const Npc&)` returns a
**squared** distance (`game/world/objects/npc.h:133`, used by `npc_getdisttonpc` at
`game/game/gamescript.cpp:2380`), and `findNpc(...)` resolves the script handles. The
reimplementation is a single comparison against the squared threshold 250000 — no new
subsystem, no sqrt.

### 1. Header declaration — `game/game/gamescript.h` (after line 306)

OLD:
```cpp
    int  npc_getdisttonpc    (std::shared_ptr<zenkit::INpc> aRef, std::shared_ptr<zenkit::INpc> bRef);
```
NEW:
```cpp
    int  npc_getdisttonpc    (std::shared_ptr<zenkit::INpc> aRef, std::shared_ptr<zenkit::INpc> bRef);
    bool npc_isnear          (std::shared_ptr<zenkit::INpc> aRef, std::shared_ptr<zenkit::INpc> bRef);
```

### 2. Binding — `game/game/gamescript.cpp` (after line 182)

OLD:
```cpp
  bindExternal("npc_getdisttonpc",               &GameScript::npc_getdisttonpc);
```
NEW:
```cpp
  bindExternal("npc_getdisttonpc",               &GameScript::npc_getdisttonpc);
  bindExternal("npc_isnear",                     &GameScript::npc_isnear);
```

### 3. Implementation — `game/game/gamescript.cpp` (after `npc_getdisttonpc`, line 2384)

NEW:
```cpp
bool GameScript::npc_isnear(std::shared_ptr<zenkit::INpc> aRef, std::shared_ptr<zenkit::INpc> bRef) {
  auto a = findNpc(aRef);
  auto b = findNpc(bRef);
  if(a==nullptr || b==nullptr)
    return false;
  // NOTE: in original-game Npc_IsNear (Gothic2.exe FUN_006f1f70 -> oCNpc::IsNear @0x0073fcc0)
  // returns TRUE when the squared 3D distance between the two NPCs is < 250000 (radius 500 units);
  // FALSE when either handle is invalid. qDistTo() already returns the squared distance.
  return a->qDistTo(*b) < 250000.f;
  }
```

### Grep-verification of referenced symbols
- `GameScript::findNpc` — used throughout `gamescript.cpp` (e.g. `2374-2375`).
- `Npc::qDistTo(const Npc&)` — declared `game/world/objects/npc.h:133`, returns squared distance (mirrors `npc_getdisttonpc`).
- `bindExternal` template — `game/game/gamescript.h:203`.
- Threshold: `python3 struct.unpack('<f', 0x48742400) == 250000.0` (confirmed).
