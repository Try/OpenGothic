# Issue #946 — setSkeleton animation adjustment is not implemented (SIGSEGV)

- **Category:** animation
- **Disposition:** FIX (surgical, low-risk null-guard)

## Issue
Crash (SIGSEGV) on Linux/RADV after the log line
`WARNING: setSkeleton animation adjustment is not implemented`.
Reported stack: script external (`GameScript::bindExternal`) → inventory item use
(`Inventory::use` / `InventoryMenu::onItemAction`) → `Npc::addOverlay(Skeleton const*, ...)`
→ `World::tickCount()`. Owner notes the warning itself is likely unrelated and the
path is never hit in unmodded play (mod content involved).

## OG files
- `game/graphics/mesh/pose.cpp` — `Pose::setSkeleton` (the warning + the crash)
- `game/graphics/mdlvisual.cpp:57` — `MdlVisual::setVisual` (`Mdl_SetVisual`)
- `game/graphics/mesh/animationsolver.cpp:57` — `AnimationSolver::addOverlay` (already null-safe)

## Original behavior (prose)
`zCModel::LoadVisualVirtual` (Gothic2.exe @ 0x00578760) loads the model prototype via
`zCModelPrototype::Load` and **guards on null**: it only constructs/returns a model when
the prototype is non-null, otherwise it returns null cleanly. The original never
dereferences a null model/skeleton when a visual fails to resolve; downstream consumers
treat a missing visual as "no skeleton" rather than crashing. So in the original, applying
a visual/overlay whose skeleton fails to load is a no-op, not a fault.

## OG current (file:line) — the divergence
`game/graphics/mesh/pose.cpp:151`
```
trY = skeleton->rootTr.y;
```
This line dereferences `skeleton` **unconditionally**, even though the same function
explicitly handles `skeleton==nullptr` immediately above (lines 144–148: `numBones`) and
below (lines 157–158: `mkSkeleton`). `MdlVisual::setVisual(v)` (mdlvisual.cpp:60-61) forwards
`v` to `solver.setSkeleton(v)` and `skInst->setSkeleton(v)` with **no null check**; when a
mod sets a visual whose skeleton resource resolves to `nullptr` (e.g. missing/invalid
`.MDH`/`.MDS` referenced by the item-use animation), `Pose::setSkeleton(nullptr)` reaches
line 151 and dereferences null → SIGSEGV. This matches the report: triggered by modded
item-use, "never hit in unmodded play".

`AnimationSolver::addOverlay` (animationsolver.cpp:58-62) already guards `sk==nullptr` and
`baseSk==nullptr`, so the overlay path is safe; the unguarded path is the Pose visual swap.

## Divergence summary
OG unconditionally reads `skeleton->rootTr.y` in `Pose::setSkeleton`, whereas the original
engine tolerates a null/unresolved visual without faulting. A null skeleton is a legal
state here (handled everywhere else in the same function).

## Proposed patch
File: `game/graphics/mesh/pose.cpp`

OLD:
```cpp
  for(auto& i:hasSamples)
    i = S_None;
  trY          = skeleton->rootTr.y;
  needToUpdate = true;
```
NEW:
```cpp
  for(auto& i:hasSamples)
    i = S_None;
  // NOTE: in original-game zCModel::LoadVisualVirtual (Gothic2.exe @ 0x00578760)
  // null-guards a failed visual/skeleton load and returns cleanly instead of
  // dereferencing it; mirror that — a null skeleton is a valid "no visual" state
  // (handled by the numBones/mkSkeleton branches above and below).
  trY          = (skeleton!=nullptr) ? skeleton->rootTr.y : 0.f;
  needToUpdate = true;
```

Risk: minimal. The branch only changes behavior in the previously-crashing null case,
preserving identical behavior for all non-null skeletons. The "not implemented" warning at
line 154 is a separate, benign TODO and is left untouched.
