# Npc_GetDistToPlayer returns exact Euclidean distance instead of the original's octagonal approximation

**Confidence:** High

## Original function + address

`Npc_GetDistToPlayer` external handler — `Gothic2.exe @0x006f3180`.

The handler resolves the NPC vob and the player vob (`oCGame::GetSelfPlayerVob`).
When both are valid it computes the distance via **`zCVob::GetDistanceToVobApprox`**
(`@0x0061b970`), truncates with `__ftol`, and returns it. When either vob is null it
returns the pre-initialised `0x7fffffff` (INT_MAX).

Crucially, `GetDistanceToVobApprox` does **not** return the true Euclidean length. It
takes the absolute per-axis position deltas `a=|dx|, b=|dy|, c=|dz|` (vob world-translation
columns at trafo offsets 0x48/0x58/0x68), sorts them so `m=max(a,b,c)`, and returns the
"octagonal" fast-distance approximation:

- `(sumSmaller)*0.125 + (sumSmaller)*0.25 + (m - 0.0625*m)`
- i.e. `0.375*(a+b+c-m) + 0.9375*m`
- equivalently `0.375*(a+b+c) + 0.5625*max(a,b,c)`

This systematically **underestimates** the true distance (by ~6.25% along an axis,
~7% on a 2D diagonal, ~2.5% on a 3D diagonal), so script tests of the form
`if(Npc_GetDistToPlayer(self) < RANGE)` trigger at a slightly larger true separation
in the original than a code path using the exact length would.

Note the contrast within the same external family: `Npc_GetDistToNpc` (`@0x006f27a0`)
and `Npc_GetDistToItem` (`@0x006f2fb0`) both use the **exact** `zCVob::GetDistanceToVob`
(`@0x0061b910`). Only `Npc_GetDistToPlayer` uses the *Approx* variant. This is distinct
from the already-fixed `GetDistToNpc` edge.

## OpenGothic file:line

`game/game/gamescript.cpp:3008` — `GameScript::npc_getdisttoplayer`.

OG computes `dp = pl->position()-npc->position();` then `l = dp.length();` (exact
Euclidean norm) and returns `int32_t(l)`. There is no octagonal approximation, so OG
reports a larger value than the original for any off-axis player/NPC offset.

## Divergence

For the same world geometry, OpenGothic's `Npc_GetDistToPlayer` returns a value up to
~7% larger than `Gothic2.exe`. Distance-gated perception/AI script logic comparing the
result against fixed thresholds therefore activates at a different separation than in
the original game.

## Proposed patch

Replace the exact length with the original's octagonal approximation while keeping the
INT_MAX invalid-arg return and the overflow clamp.

OLD (`game/game/gamescript.cpp:3008`):
```cpp
int GameScript::npc_getdisttoplayer(std::shared_ptr<zenkit::INpc> npcRef) {
  auto pl  = world().player();
  auto npc = findNpc(npcRef);
  if(pl==nullptr || npc==nullptr) {
    return std::numeric_limits<int32_t>::max();
    }
  auto dp = pl->position()-npc->position();
  auto l  = dp.length();
  if(l>float(std::numeric_limits<int32_t>::max())) {
    return std::numeric_limits<int32_t>::max();
    }
  return int32_t(l);
  }
```

NEW:
```cpp
int GameScript::npc_getdisttoplayer(std::shared_ptr<zenkit::INpc> npcRef) {
  auto pl  = world().player();
  auto npc = findNpc(npcRef);
  if(pl==nullptr || npc==nullptr) {
    return std::numeric_limits<int32_t>::max();
    }
  auto dp = pl->position()-npc->position();
  // NOTE: in original-game Npc_GetDistToPlayer (Gothic2.exe 0x006f3180) the distance is
  // computed with zCVob::GetDistanceToVobApprox (0x0061b970), an octagonal fast-distance
  // approximation, NOT the exact Euclidean length: 0.375*(|dx|+|dy|+|dz|) + 0.5625*max(...).
  // (Npc_GetDistToNpc/Npc_GetDistToItem use the exact zCVob::GetDistanceToVob instead.)
  const float ax = std::abs(dp.x), ay = std::abs(dp.y), az = std::abs(dp.z);
  const float mx = std::max(ax,std::max(ay,az));
  const float l  = 0.375f*(ax+ay+az) + 0.5625f*mx;
  if(l>float(std::numeric_limits<int32_t>::max())) {
    return std::numeric_limits<int32_t>::max();
    }
  return int32_t(l);
  }
```

`std::abs`/`std::max` are already used throughout `gamescript.cpp`; `dp` is a
`Tempest::Vec3` with `.x/.y/.z` members (used directly elsewhere in this file).
