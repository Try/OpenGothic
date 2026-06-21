# Mdl_ApplyOverlayMds: missing duplicate-overlay dedup

**Confidence:** Medium

## Original function + address

`oCNpc::ApplyOverlay(zSTRING const&)` at `0072d2c0` is the engine routine
behind the `Mdl_ApplyOverlayMds` external. Before applying, it uppercases the
overlay-MDS name and **scans the NPC's persistent overlay-name list** (the
`zCArray<zSTRING>` at NPC offset `0x928`/count `0x930`). If a matching name is
already present, it returns early (success) **without applying the overlay a
second time and without adding a second list entry**. Only when the name is not
already present does it call `zCModel::ApplyModelProtoOverlay` and append the
name to the list. In other words, applying the same overlay MDS twice via
`Mdl_ApplyOverlayMds` is idempotent in the original game: exactly one overlay
entry exists.

Note: the *timed* path `oCNpc::ApplyTimedOverlayMds` (`00756890`) does NOT
perform this dedup; only the plain `Mdl_ApplyOverlayMds` (time == 0) path does.

## OpenGothic location

`game/graphics/mesh/animationsolver.cpp:57` `AnimationSolver::addOverlay`
unconditionally `overlay.push_back(ov)` (line 66) with no duplicate check. Both
`GameScript::mdl_applyoverlaymds` and `mdl_applyoverlaymdstimed`
(`game/game/gamescript.cpp:1923`/`1931`) funnel through `Npc::addOverlay` ->
`MdlVisual::addOverlay` -> this method.

## Divergence

A script that calls `Mdl_ApplyOverlayMds` for the same MDS more than once
(common in re-applied weapon/stance/talent overlays) ends up with N entries in
OpenGothic but only 1 in the original. A single matching `Mdl_RemoveOverlayMds`
then removes only one entry (`delOverlay` deletes the first match and returns),
so the overlay stays active in OpenGothic while it is fully gone in the original
-- a visible animation-set difference.

## Proposed patch

```cpp
// game/graphics/mesh/animationsolver.cpp  (AnimationSolver::addOverlay)
// OLD:
void AnimationSolver::addOverlay(const Skeleton* sk, uint64_t time) {
  if(sk==nullptr)
    return;
  // incompatible overlay
  if(baseSk==nullptr || sk->nodes.size()!=baseSk->nodes.size())
    return;
  Overlay ov;
  ov.skeleton = sk;
  ov.time     = time;
  overlay.push_back(ov);
  invalidateCache();
  }

// NEW:
void AnimationSolver::addOverlay(const Skeleton* sk, uint64_t time) {
  if(sk==nullptr)
    return;
  // incompatible overlay
  if(baseSk==nullptr || sk->nodes.size()!=baseSk->nodes.size())
    return;
  // NOTE: in original-game oCNpc::ApplyOverlay (0072d2c0) the non-timed
  // Mdl_ApplyOverlayMds path is idempotent: it scans the persistent overlay
  // name-list and skips re-adding a duplicate. The timed path does not dedup,
  // so only guard time==0 here.
  if(time==0 && hasOverlay(sk))
    return;
  Overlay ov;
  ov.skeleton = sk;
  ov.time     = time;
  overlay.push_back(ov);
  invalidateCache();
  }
```
