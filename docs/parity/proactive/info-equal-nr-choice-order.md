# Dialog choice ordering: tie-break for equal `nr` infos diverges from original

**Confidence:** Medium-High

## Original function + address (prose only)

The original info list is sorted once, at construction, by `oCInfoManager::oCInfoManager`
(`0x007023f0`). For every `C_INFO` instance returned by `zCParser::GetInstance` (which
enumerates instances in ascending instance-symbol-index = script-declaration order), the
manager performs an **insertion sort into a singly-linked list** using the comparator
`oCInfoManager::CompareInfos` (`0x007026f0`).

`CompareInfos(newInfo, existing)` reads only the `nr`/priority field (struct offset `0x20`)
and returns:
- `+1` when `existing.nr < newInfo.nr`,
- `0` when `newInfo.nr == existing.nr`,
- `-1` when `newInfo.nr < existing.nr`.

The ctor's insertion loop links the new info *before* an existing node only when the
comparator returns `< 0` (strictly smaller `nr`); for an equal `nr` it returns `0`, so the
new info keeps scanning and is placed **after** all already-present infos of the same `nr`.
This makes the original a **stable sort by `nr` only**, with ties broken by
declaration/registration order (ascending info-instance symbol index).

`oCInformationManager::CollectInfos` (`0x00661aa0`) then walks this pre-sorted list via
`GetInfoCountUnimportant`/`GetInfoUnimportant` (`0x00702c00` / `0x00703030`) and pushes
choices to the view in that exact list order. There is no second sort and no tie-break on
the info's *information function* symbol.

## OpenGothic file:line

- `game/game/gamescript.cpp:863-922` `GameScript::dialogChoices` (sets `ch.sort = info.nr`,
  `ch.scriptFn = info.information`, then calls `sort(choice)`).
- `game/game/gamescript.cpp:3438-3442` `GameScript::sort` (the shared comparator).

```
3439  std::sort(dlg.begin(),dlg.end(),[](const GameScript::DlgChoice& l,const GameScript::DlgChoice& r){
3440    return std::tie(l.sort,l.scriptFn)<std::tie(r.sort,r.scriptFn); // small hack with scriptfn to reproduce behavior of original game
3441    });
```

## Divergence

`dialogsInfo` is already populated in registration order
(`enumerate_instances_by_class_name("C_INFO", ...)`, `gamescript.cpp:446`), which matches
the order the original feeds into its stable insertion sort. `dialogChoices` iterates it in
that order and builds `choice` accordingly. Two problems then break parity for infos that
share the same `nr`:

1. `GameScript::sort` uses **`std::sort`, which is not stable**. For equal `nr` the relative
   order of the already-correctly-ordered input is not guaranteed to be preserved.
2. To compensate, the comparator adds a secondary key `scriptFn = info.information` — the
   **information function** symbol index (e.g. `DIA_X_Hello_Info`), which is a *different*
   symbol from the C_INFO instance the original tie-breaks on (e.g. `DIA_X_Hello`). These
   two indices usually run parallel (function declared right after its instance), which is
   why the "hack" mostly works, but they can diverge when `_Info` functions are declared in
   a different order than their instances. In those cases OpenGothic orders equal-`nr`
   choices by function index while the original orders them by instance index.

Equal-`nr` infos are common in Gothic II scripts (shared priority buckets, default `nr`,
trade/permanent infos), so the ordering of several visible choice lines can differ.

The sub-choice path (`updateDialog`, `gamescript.cpp:933-944`) is unaffected: there
`ch.sort = int(i)` is unique and monotonic, so the `scriptFn` tie-break never fires and the
order is identical under either sort.

## Proposed patch

Replace the non-stable, function-index tie-break with a **stable sort by `sort` only**,
which exactly reproduces the original's stable-insertion-sort-by-`nr` over the
registration-ordered input. This is safe for both call sites (`dialogChoices`: input is in
registration order; `updateDialog`: `sort` is already a unique index).

`game/game/gamescript.cpp:3438`

OLD:
```cpp
void GameScript::sort(std::vector<GameScript::DlgChoice> &dlg) {
  std::sort(dlg.begin(),dlg.end(),[](const GameScript::DlgChoice& l,const GameScript::DlgChoice& r){
    return std::tie(l.sort,l.scriptFn)<std::tie(r.sort,r.scriptFn); // small hack with scriptfn to reproduce behavior of original game
    });
  }
```

NEW:
```cpp
void GameScript::sort(std::vector<GameScript::DlgChoice> &dlg) {
  // NOTE: in original-game oCInfoManager::oCInfoManager @0x007023f0 the info list is built by a
  // stable insertion sort using oCInfoManager::CompareInfos @0x007026f0, which compares only `nr`
  // and keeps registration (instance-symbol) order for equal `nr`. dialogsInfo here is already in
  // registration order, so a stable sort by `sort` (=nr) alone reproduces that; the previous
  // scriptFn (information-function index) tie-break could reorder equal-nr infos.
  std::stable_sort(dlg.begin(),dlg.end(),[](const GameScript::DlgChoice& l,const GameScript::DlgChoice& r){
    return l.sort<r.sort;
    });
  }
```

`scriptFn` is still required as a field (it carries the info/information function id used by
`exec`), so no struct change is needed. `<algorithm>` is already included transitively;
`std::stable_sort` lives there alongside the existing `std::sort` usage.
