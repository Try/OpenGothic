# TouchDamage repeat_delay==0 deals damage every frame instead of once-per-entry

**Confidence:** Medium-High

## Original behavior

`zCTouchDamage::OnTouch` (Gothic2.exe @ 0x00615b70) and
`zCTouchDamage::OnTimer` (@ 0x00615c70), repeat field at vob offset 0x128
(`repeatDelaySec`, in seconds; converted to ms via *1000.0):

- `OnTouch` fires one damage message immediately when a vob enters the trigger
  volume. It then arms the repeat timer (`SetOnTimer(repeatDelaySec*1000)`) **only
  if `repeatDelaySec > 0`** and no timer is already running.
- `OnTimer` re-tests all touching vobs, fires one damage message to each, and
  re-arms the timer **only if `repeatDelaySec > 0`**; otherwise the vob goes to
  sleep (`SetSleeping(1)` when nothing is touching).
- Consequence: with `repeatDelaySec == 0` (which is also the constructor default,
  offset 0x128 initialized to 0), the original deals damage **exactly once** per
  entry into the volume — it never re-arms, so no per-frame repetition.

## OpenGothic behavior

`game/world/triggers/touchdamage.cpp` `TouchDamage::tick` (lines 29-58):

```cpp
if(world.tickCount()<=repeatTimeout)
  return;
... apply damage to all intersecting npcs ...
repeatTimeout = world.tickCount() + uint64_t(repeatDelaySec*1000);
```

`onIntersect` calls `enableTicks()`, so `tick` runs every frame while an NPC is
inside the volume.

## The divergence

When `repeatDelaySec == 0`:
- `repeatTimeout` becomes `world.tickCount()` after a hit.
- On the next frame `world.tickCount() > repeatTimeout`, so the gate passes and
  damage is applied **again** — every single frame the NPC stays inside.

So an OpenGothic touch-damage zone with `repeat_delay_sec == 0` is a continuous
per-frame DoT, whereas in original Gothic II it is a single hit on entry. This
makes such zones (including any using the default delay) drastically more lethal.

## Proposed patch

File: `game/world/triggers/touchdamage.cpp`

Patch 1 — `TouchDamage::tick`:

OLD:
```cpp
  repeatTimeout = world.tickCount() + uint64_t(repeatDelaySec*1000);

  if(intersections().empty())
    disableTicks();
```

NEW:
```cpp
  // NOTE: in original-game (zCTouchDamage::OnTouch/OnTimer @ 0x00615b70/0x00615c70)
  // the repeat timer is armed only when repeatDelaySec>0; with repeatDelaySec==0
  // damage is dealt exactly once per entry, not every frame.
  if(repeatDelaySec>0)
    repeatTimeout = world.tickCount() + uint64_t(repeatDelaySec*1000);
  else
    repeatTimeout = std::numeric_limits<uint64_t>::max(); // one-shot until re-entry

  if(intersections().empty())
    disableTicks();
```

Patch 2 — `TouchDamage::onIntersect` (reset so a fresh entry re-triggers the
one-shot hit; currently `repeatTimeout` is never cleared once a vob leaves):

OLD:
```cpp
void TouchDamage::onIntersect(Npc& n) {
  AbstractTrigger::onIntersect(n);
  enableTicks();
  }
```

NEW:
```cpp
void TouchDamage::onIntersect(Npc& n) {
  AbstractTrigger::onIntersect(n);
  if(intersections().size()==1) // first occupant after being empty
    repeatTimeout = 0;          // allow the one-shot/initial hit again
  enableTicks();
  }
```

(Requires `#include <limits>` in touchdamage.cpp.)
