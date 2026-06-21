# Focus picking ignores the vertical elevation cone (elevdo/elevup)

**Confidence:** Medium-High (divergence is certain; patch is data-robust but the
exact elevation reference frame is approximated — see "Caveat").

## Original function + address (prose)

In `Gothic2.exe` the on-screen focus / interaction target is computed by
`oCAIHuman::CheckFocusVob` @ `0x0069b7xx`, which calls `oCNpc::CollectFocusVob`
@ `0x00733xxx`. For every candidate vob `CollectFocusVob` calls
`oCNpc::FocusCheck` @ `0x007331xx` (or `oCNpc::FocusCheckBBox` for bbox vobs).
`FocusCheck` first range-gates via `oCNpcFocus::IsInRange`, then computes the
horizontal *and vertical* angles to the candidate with `oCNpc::GetAngles`
(via `Alg_CalcAziElevUnit`, producing azimuth and **elevation** in degrees,
relative to the player's forward at-vector), and finally gates with
`oCNpcFocus::IsInAngle`.

`oCNpcFocus::IsInAngle(type, azimuth, elevation)` (file `oFocus.cpp`) checks
**two** angles, per vob type, against the loaded `C_Focus` instance:

- NPC (type `0x82`): `abs(azimuth) < npc_azi` (struct +0x0c) **and**
  `npc_elevdo (+0x10) < elevation < npc_elevup (+0x14)`.
- Item (type `0x81`): `abs(azimuth) < item_azi` (+0x24) **and**
  `item_elevdo (+0x28) < elevation < item_elevup (+0x2c)`.
- Mob (type `0x80`): `abs(azimuth) < mob_azi` (+0x3c) **and**
  `mob_elevdo (+0x40) < elevation < mob_elevup (+0x44)`.

(Struct offsets confirmed via `oCNpcFocus::GetAzi` returning `this+0x0c`, and the
field order in `zenkit::IFocus`: `*_range1, *_range2, *_azi, *_elevdo, *_elevup,
*_prio`.)

With stock Gothic 2 `Focus.d` the elevation cone is meaningful for **items**
(`item_elevdo=-40, item_elevup=40`) and **mobs** (`mob_elevdo=-40,
mob_elevup=40`); NPCs use a wide `-90/+90`. So vanilla refuses to focus an item
or interactive mob that is more than ~40 degrees above or below the player's
view line (e.g. a lever high on a wall, an item at your feet) while you are
looking level.

## OpenGothic file:line

`game/world/worldobjects.cpp:1093` — `WorldObjects::testObj(...)`. The angle gate
is lines 1097-1118:

```cpp
const float ang   = float(std::cos(double(opt.azi)*M_PI/180.0));
...
auto pos   = npc.position();
auto dpos  = pos - pl.position();
auto angle = std::atan2(dpos.z,dpos.x);
if(std::cos(plAng-angle)<ang && !bool(opt.flags&SearchFlg::NoAngle))
  return false;
```

Only the **horizontal** azimuth cone is enforced. `dpos.y` (the vertical
component) is never used, and the `IFocus::*_elevdo` / `*_elevup` fields are never
read anywhere in OpenGothic (grep: only `*_azi`, `*_range1`, `*_range2`,
`*_prio` are referenced — `game/world/world.cpp:422-424,482`). The vertical
elevation cone is therefore missing entirely.

## Divergence

OpenGothic focuses items and mobs at arbitrary vertical angles, whereas the
original drops them from focus once they leave the `[*_elevdo, *_elevup]` cone
(±40 degrees for items/mobs in stock data). Result: OG highlights / lets you
interact with high or low items and mobs that vanilla would not consider the
focus while you look level.

## Proposed patch

Add an elevation cone to `SearchOpt` (permissive default so non-focus searches
are unchanged), populate it from the focus policy, and gate on it in `testObj`.

### 1) `game/world/worldobjects.h` (struct `SearchOpt`, ~line 43)

OLD:
```cpp
      float         rangeMin    = 0;
      float         rangeMax    = 0;
      float         azi         = 0;
```
NEW:
```cpp
      float         rangeMin    = 0;
      float         rangeMax    = 0;
      float         azi         = 0;
      // NOTE: in original-game oCNpcFocus::IsInAngle (Gothic2.exe @0x007331xx via
      // oCNpc::GetAngles) focus also gates the vertical angle to the candidate
      // against [elevdo,elevup]; default cone is fully open so non-focus searches
      // (e.g. movement mob search) keep their old behaviour.
      float         elevLo      = -180;
      float         elevHi      =  180;
```

### 2) `game/world/world.cpp` — `World::findFocus(const Npc&, const Focus&)` (after line 424)

OLD:
```cpp
  WorldObjects::SearchOpt optNpc {policy.npc_range1,  policy.npc_range2,  policy.npc_azi,  collAlgo, collType, opt};
  WorldObjects::SearchOpt optMob {policy.mob_range1,  policy.mob_range2,  policy.mob_azi,  collAlgo};
  WorldObjects::SearchOpt optItm {policy.item_range1, policy.item_range2, policy.item_azi, collAlgo, collType};
```
NEW:
```cpp
  WorldObjects::SearchOpt optNpc {policy.npc_range1,  policy.npc_range2,  policy.npc_azi,  collAlgo, collType, opt};
  WorldObjects::SearchOpt optMob {policy.mob_range1,  policy.mob_range2,  policy.mob_azi,  collAlgo};
  WorldObjects::SearchOpt optItm {policy.item_range1, policy.item_range2, policy.item_azi, collAlgo, collType};
  // NOTE: in original-game oCNpcFocus::IsInAngle (Gothic2.exe) the focus gate also
  // rejects candidates outside the per-type vertical elevation cone.
  optNpc.elevLo = policy.npc_elevdo;  optNpc.elevHi = policy.npc_elevup;
  optMob.elevLo = policy.mob_elevdo;  optMob.elevHi = policy.mob_elevup;
  optItm.elevLo = policy.item_elevdo; optItm.elevHi = policy.item_elevup;
```

(Apply the same two lines for `optNpc` to the second `findFocus`/`testFocusNpc`
helper at `world.cpp:482` if NPC-only elevation parity there is also wanted; for
stock data NPC elev is ±90 so it is a no-op.)

### 3) `game/world/worldobjects.cpp` — `testObj(...)` (after the azimuth gate, ~line 1118)

OLD:
```cpp
  if(std::cos(plAng-angle)<ang && !bool(opt.flags&SearchFlg::NoAngle))
    return false;

  l = std::sqrt(l);
```
NEW:
```cpp
  if(std::cos(plAng-angle)<ang && !bool(opt.flags&SearchFlg::NoAngle))
    return false;

  // NOTE: in original-game oCNpcFocus::IsInAngle (Gothic2.exe) focus also gates the
  // vertical angle: elevdo < elevation < elevup (degrees), positive = above.
  if(!bool(opt.flags&SearchFlg::NoAngle)) {
    const float horiz = std::sqrt(dpos.x*dpos.x + dpos.z*dpos.z);
    const float elev  = float(std::atan2(double(dpos.y),double(horiz))*180.0/M_PI);
    if(elev<=opt.elevLo || elev>=opt.elevHi)
      return false;
    }

  l = std::sqrt(l);
```

## Caveat

The original derives elevation from the player's forward **at-vector** (via
`oCNpc::GetAngles`/`Alg_CalcAziElevUnit`), which for a normally-standing actor is
horizontal, so `atan2(dy, horizDist)` reproduces it; on special poses
(throne/bench) the original tilts the reference, which this patch does not model.
The patch is deliberately gated behind `!NoAngle` so movement-time interactive
search (`optMvMob`, which sets `NoAngle`) is unaffected, and the permissive
`elevLo=-180/elevHi=180` default keeps every non-focus `SearchOpt` user (spell
caster collection, `findInteractive(pl)`) byte-for-byte unchanged.
