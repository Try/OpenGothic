# Focus within-type candidate is picked by nearest distance instead of smallest azimuth angle

**Confidence:** High (on the divergence). Medium on the proposed patch being fully regression-free (one shared NoAngle code path needs scoping — see patch).

## Original function + address (prose)

In `Gothic2.exe` the player target-highlight is built by `oCNpc::CollectFocusVob` (@0x00733a10).
It clears the old focus, builds a candidate vob list out to `oCNpcFocus::GetMaxRange`
(@0x006bef70), then walks the list. Before scanning it seeds a running "best azimuth angle"
local to **181.0 degrees** (a sentinel larger than any real azimuth, so the first valid
candidate always wins) and a running "best type" of 0. For each candidate it calls
`oCNpc::FocusCheck` (@0x007331c0) / `oCNpc::FocusCheckBBox` (@0x00732f40), passing the
current best angle and best type by value/reference.

`FocusCheck` first range-gates via `oCNpcFocus::IsInRange` (@0x006bf070), then computes the
candidate's azimuth and elevation with `oCNpc::GetAngles` and angle-gates via
`oCNpcFocus::IsInAngle` (@0x006bf100, which checks `abs(azimuth) < azi` **and**
`elevdo < elevation < elevup`). The selection metric is then purely angular: it compares
`oCNpcFocus::GetPriority` (@0x006bf030) of the candidate's type vs the current best type, and
**when the priorities are equal it keeps the candidate only if its absolute azimuth angle is
strictly smaller than the running best** (the two `abs(__ftol(...))` comparisons in
`FocusCheck`; reject/return 0 otherwise). On success `CollectFocusVob` does
`bestAngle = candidateAngle` and records the new focus. Distance is used **only** for the
range gate — never as the within-type selection key. So within a single type
(all NPCs are type 0x82, all items 0x81, all mobs 0x80) the original selects the candidate
**most centered to the NPC's facing direction (smallest |azimuth|)**, not the nearest one.
Cross-type ordering is handled separately by the per-type `*_prio` fields.

## OpenGothic file:line

`/Users/admin/Downloads/opengothic/game/world/worldobjects.cpp:1103-1136`
(`WorldObjects::testObj<T>(... , float& rlen)`), used by `findObj` (line 1078), `findItem`
(784), `findInteractive` (747) — i.e. the within-type pick for `World::findFocus`
(`game/world/world.cpp:416`).

## Divergence

OpenGothic's `testObj` gates by range and by the azimuth cone (`std::cos(plAng-angle) < ang`),
but then selects the winner by **nearest linear distance**:

```cpp
l = std::sqrt(l);
if(l<rlen && (bool(opt.flags&SearchFlg::NoRay) || canSee(pl,npc))){
  rlen=l;            // threaded across candidates -> keeps the closest
  return true;
  }
```

`rlen` (seeded by each caller to `rangeMax*rangeMax`) is the running "best" key and tracks the
**minimum distance**. The original instead tracks the **minimum absolute azimuth angle**
(seed 181 deg). Consequence: with two valid candidates in the cone, OpenGothic highlights the
physically closer one while the original highlights the one the player is more directly facing.
This is observable whenever a near off-axis vob and a farther on-axis vob are both in the cone
(e.g. looting / talking: vanilla locks onto whoever you point the camera at, OG snaps to the
nearest body). OpenGothic additionally drops the elevation (`elevdo/elevup`) gate entirely, but
that is a separate, secondary issue; the selection-key mismatch is the primary divergence.

## Proposed patch

Change the within-type selection key from distance to azimuth alignment, reusing the cosine
already computed for the cone gate. Keep the existing distance key for the `NoAngle` path only
(that flag is used by the non-focus vob-move search `World::findInteractive`,
`game/world/world.cpp:490`, and by the def-revalidation in `findNpcNear`, which is single-
candidate so its key is irrelevant) so this stays scoped to the focus pick.

OLD (`game/world/worldobjects.cpp`, inside `testObj(..., float& rlen)`):
```cpp
  if(std::cos(plAng-angle)<ang && !bool(opt.flags&SearchFlg::NoAngle))
    return false;

  l = std::sqrt(l);
  if(l<rlen && (bool(opt.flags&SearchFlg::NoRay) || canSee(pl,npc))){
    rlen=l;
    return true;
    }
  return false;
```

NEW:
```cpp
  const float c = float(std::cos(double(plAng-angle)));
  if(c<ang && !bool(opt.flags&SearchFlg::NoAngle))
    return false;

  // NOTE: in original-game oCNpc::CollectFocusVob @0x00733a10 / oCNpc::FocusCheck @0x007331c0
  // the within-type focus candidate is selected by smallest absolute azimuth (most centered to
  // the NPC facing, sentinel 181deg), not by nearest distance. Use angular alignment as the key
  // for focus searches; keep the distance key only for the NoAngle (vob-move) path.
  float key = std::sqrt(l);
  if(!bool(opt.flags&SearchFlg::NoAngle))
    key = -c; // smaller (== more aligned) wins; seed rlen (>=rangeMin^2) is a valid worst-case
  if(key<rlen && (bool(opt.flags&SearchFlg::NoRay) || canSee(pl,npc))){
    rlen=key;
    return true;
    }
  return false;
```

This is a single-site change. The callers seed `rlen = opt.rangeMax*opt.rangeMax` (a large
positive number), which remains a valid "worst-case" sentinel for the `-cos` key (range [-1,1]),
and they only read `rlen` as the running-best comparator — they never consume its distance value
elsewhere — so no caller edits are required.

Verified OG symbols: `WorldObjects::testObj` / `rlen` / `opt.flags` / `SearchFlg::NoAngle` /
`SearchFlg::NoRay` (worldobjects.h:31-48), `pl.rotationRad()` (npc.h:100),
`pl.qDistTo()` (npc.h:131), `canSee(...)` helpers (worldobjects.cpp:1062-1075).
