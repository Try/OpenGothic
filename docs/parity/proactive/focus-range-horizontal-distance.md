# Focus range gate uses 3D distance instead of horizontal (XZ) distance

**Confidence:** High (diagnosis); Medium-High (patch — surgical, build-verifiable, but the
original additionally subtracts the target bounding-box extent, see "Known remaining delta").

## Original function + address

`oCNpc::FocusCheck` @ `0x007331c0` (called from `oCNpc::CollectFocusVob` @ `0x00733a10`,
which is the per-frame focus/auto-aim collector). Before testing range via
`oCNpcFocus::IsInRange` @ `0x006bf070`, FocusCheck builds the player-to-target offset vector
with the **vertical component explicitly forced to zero**:

```
zVEC3(&d, player.x - target.x, 0.0f, player.z - target.z);   // Y hard-coded to 0.0
dist2 = d.x*d.x + 0 + d.z*d.z;                                // horizontal quad-distance
half  = max( (target.bbox.max.z - target.bbox.min.z)*0.5f,
             (target.bbox.max.x - target.bbox.min.x)*0.5f );  // larger horizontal half-extent
dist2 = max(dist2 - half*half, 0.0f);                         // distance to bbox edge, clamped
IsInRange(focusMode, dist2);
```

So the focus **range** gate is a purely horizontal (XZ-plane) distance between the player and
target object origins; elevation/Y separation never affects whether a target is in focus range.
(`IsInRange` then compares `dist2` to the per-mode `range1²`/`range2²` from the selected
`C_Focus` instance. The squaring is done once in `oCNpcFocus::Init` @ `0x006bee70`.)

## OG file:line

`/Users/admin/Downloads/opengothic/game/world/worldobjects.cpp:1119` — `WorldObjects::testObj`:

```cpp
float l = pl.qDistTo(npc);
if(l>qmax || l<qmin)
  return false;
```

`Npc::qDistTo` (`game/world/objects/npc.cpp:736-758`) returns the **full 3D** squared distance
(`(target - centerPosition()).quadLength()`), including the vertical delta and using the
mid-height `centerPosition()` for the player.

## Divergence

OpenGothic gates focus range on the slant (3D) distance, while the original measures only the
horizontal ground distance. Consequence: a target that is well within horizontal range but
vertically displaced (on stairs, a ledge, a wall walkway, a balcony, or simply much taller/shorter
so its center is offset) reads as *farther away* in OpenGothic and can be pushed past `range2`,
silently dropping the focus / auto-aim that the original keeps. This is the "focus elevation gate"
behavior and is distinct from the already-deferred *focus-elevation-cone* (the `IsInAngle`
vertical-angle gate `npc_elevdo`/`npc_elevup`); this one is the **range** gate, not the angle gate.

## Proposed patch

Replace the 3D range metric with the horizontal one. The offset vector `dpos = npc.position() -
pl.position()` is already computed a few lines below for the azimuth test and can be hoisted up;
`Vob::position()` (the object origin) is the analogue of the original's `GetTranslation`, and
horizontally matches `centerPosition()` for an NPC (the two differ only in Y).

OLD (`game/world/worldobjects.cpp`, in `testObj(..., float& rlen)`):
```cpp
  float l = pl.qDistTo(npc);
  if(l>qmax || l<qmin)
    return false;

  auto pos   = npc.position();
  auto dpos  = pos - pl.position();
  auto angle = std::atan2(dpos.z,dpos.x);
```

NEW:
```cpp
  // NOTE: in original-game oCNpc::FocusCheck @0x007331c0 the focus range gate measures the
  // horizontal (XZ-plane) distance between the player and target object origins (the Y/elevation
  // component is explicitly zeroed before squaring), so vertically-separated targets stay in range.
  // OpenGothic used full 3D qDistTo (incl. the centerPosition height delta), which shrinks the
  // effective focus range for targets on stairs/ledges/above the player. Use horizontal distance.
  auto pos   = npc.position();
  auto dpos  = pos - pl.position();
  float l = dpos.x*dpos.x + dpos.z*dpos.z;
  if(l>qmax || l<qmin)
    return false;

  auto angle = std::atan2(dpos.z,dpos.x);
```

`qmin`/`qmax` are already `rangeMin²`/`rangeMax²` (lines 1105-1106), so the comparison stays
dimensionally correct against the squared horizontal distance. The `NoAngle` (vob-move) branch's
distance key `std::sqrt(l)` becomes horizontal too, matching the same FocusCheck metric.

## Known remaining delta (not patched)

The original also subtracts the larger horizontal bounding-box half-extent from the distance
(`dist2 - half²`, clamped to 0), measuring to the target's bbox edge rather than its origin. That
is a smaller, separate refinement (matters only for large vobs near the range boundary) and would
require bbox access inside the templated `testObj`; left out to keep the fix surgical.
