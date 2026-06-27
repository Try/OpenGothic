# TouchDamage: entry hit only granted to the first occupant, not every entrant

**Confidence:** High

## Original function + address (prose)

Touch zones in ZenGin are driven by two virtual callbacks on the vob:

- `zCTouchDamage::OnTouch(zCVob*)` (Gothic2.exe `0x00615b70`) is invoked once per vob the
  moment that vob *enters* the touch volume (a collision-enter event). For the entering vob it
  runs the collision-type test and, if it passes, fires a single damage message at that one vob
  (the inlined `zCTouchDamage::FireDamageMessage` body, cf. `0x00616070`). Only after dealing
  that per-vob hit does it arm the shared repeat timer, and only when `repeat_delay > 0` and a
  timer is not already pending (`if(0 < this->repeatDelay) { if(!IsOnTimer()) SetOnTimer(repeatDelay*1000); }`).
- `zCTouchDamage::OnTimer()` (Gothic2.exe `0x00615c70`) is the shared repeat: it walks the
  *whole* current toucher list and fires the damage message at every toucher at once, then
  re-arms itself if `repeat_delay > 0`, or calls `SetSleeping(1)` when the list is empty.

The key consequence: the *immediate entry hit* is delivered **per entering vob** by `OnTouch`,
independently of the shared `OnTimer` repeat cadence. If NPC A is already standing in the zone
and NPC B walks in, B's own `OnTouch` fires and B takes an immediate hit; A's pending timer is
untouched. This holds even when `repeat_delay == 0` (no `OnTimer` is ever scheduled, so each
vob gets exactly one hit on entry, but *each* vob still gets it).

## OpenGothic file:line

`game/world/triggers/touchdamage.cpp:26` (`onIntersect`) and `:33` (`tick`).

```cpp
void TouchDamage::onIntersect(Npc& n) {
  AbstractTrigger::onIntersect(n);
  if(intersections().size()==1) // first occupant after being empty: allow the entry hit
    repeatTimeout = 0;
  enableTicks();
  }
```

## Divergence

OpenGothic delivers the entry hit through `tick()`: `onIntersect` resets `repeatTimeout = 0`
*only* on the empty→first-occupant transition (`intersections().size()==1`), so the next `tick`
damages everyone currently inside. A vob that enters while the zone is already occupied does
**not** reset `repeatTimeout`, so it receives no entry hit and instead has to wait for the
shared repeat timer.

`CollisionZone::onIntersect` (`game/world/collisionzone.cpp:137`) confirms the OG callback fires
exactly once per *new* entrant (it skips vobs already in `intersect`), so it is the precise
analogue of the original `OnTouch` collision-enter event — yet only the first entrant is acted on.

Observable effects:
- **Spike-trap / one-shot zone (`repeat_delay == 0`)**: NPC A steps on the trap and takes the
  hit; `repeatTimeout` is set to `UINT64_MAX`. While A is still on it, NPC B (or the hero) steps
  on the same trap and takes **no damage at all** in OG, because `intersections().size()` is 2 so
  `repeatTimeout` is never reset and `tick` early-returns forever. In the original B takes the hit.
- **Repeating zone (`repeat_delay > 0`)**: a second entrant misses its immediate entry hit and
  only starts taking damage at the next shared-timer expiry, delaying its first hit by up to
  `repeat_delay` seconds versus the original.

## Proposed patch

Factor the per-NPC damage application out of `tick()` into a helper, deal it to each new
entrant in `onIntersect` (the per-vob entry hit), and keep `tick()` for the shared repeat over
all current occupants. This preserves the already-fixed behaviors (they live inside the moved
block): the `repeat_delay==0` once-per-entry timer arming, and the `DAM_BARRIER`-while-swimming
instant kill.

`touchdamage.h` — declare the helper (add next to the other private methods, line 15):

```cpp
    void takeDamage(Npc& npc, int32_t val, int32_t prot);
    void applyDamage(Npc& npc);   // NEW
```

`touchdamage.cpp` — OLD (`onIntersect`, lines 26-31):

```cpp
void TouchDamage::onIntersect(Npc& n) {
  AbstractTrigger::onIntersect(n);
  if(intersections().size()==1) // first occupant after being empty: allow the entry hit
    repeatTimeout = 0;
  enableTicks();
  }
```

NEW:

```cpp
void TouchDamage::onIntersect(Npc& n) {
  AbstractTrigger::onIntersect(n);
  // NOTE: in original-game zCTouchDamage::OnTouch (Gothic2.exe 0x00615b70) is a collision-enter
  // callback fired once per *entering* vob, and it deals that vob an immediate "entry" hit
  // regardless of how many other vobs are already inside; the shared OnTimer repeat (0x00615c70)
  // is a separate mechanism. OpenGothic previously granted the entry hit only on the empty->first
  // transition, so a vob entering an already-occupied zone (e.g. a second NPC on a one-shot spike
  // trap) received no hit. Deal the entry hit per-entrant here.
  applyDamage(n);
  if(intersections().size()==1) {
    // first occupant: arm the shared repeat cadence (OnTimer). repeatDelaySec==0 -> never repeats.
    if(repeatDelaySec>0)
      repeatTimeout = world.tickCount() + uint64_t(repeatDelaySec*1000);
    else
      repeatTimeout = std::numeric_limits<uint64_t>::max();
    }
  enableTicks();
  }
```

`touchdamage.cpp` — OLD (`tick`, lines 33-91): move the per-NPC block (the `mask[...]` setup,
the `DAM_BARRIER`+`isSwim()` instant kill, and the split/protection accumulation that calls
`changeAttribute`) into the new `applyDamage(Npc&)` so the loop body becomes a single call, and
the entry-hit-via-`repeatTimeout==0` mechanism is dropped:

```cpp
void TouchDamage::applyDamage(Npc& npcRef) {
  Npc* npc = &npcRef;
  bool mask[zenkit::DamageType::NUM] = {};
  mask[zenkit::DamageType::BARRIER] = barrier;
  mask[zenkit::DamageType::BLUNT]   = blunt;
  mask[zenkit::DamageType::EDGE]    = edge;
  mask[zenkit::DamageType::FIRE]    = fire;
  mask[zenkit::DamageType::FLY]     = fly;
  mask[zenkit::DamageType::MAGIC]   = magic;
  mask[zenkit::DamageType::POINT]   = point;
  mask[zenkit::DamageType::FALL]    = fall;

  auto& hnpc = npc->handle();
  // NOTE: original oCNpc::OnDamage_Hit @0x00666610 DAM_BARRIER-while-swimming instant kill.
  if(mask[zenkit::DamageType::BARRIER] && npc->isSwim()) {
    npc->changeAttribute(ATR_HITPOINTS,-hnpc.attribute[ATR_HITPOINTS],false);
    return;
    }
  // NOTE: original ApplyDamages @0x0065e5a0 splits the scalar damage evenly across set types.
  int32_t nTypes = 0;
  for(size_t i=0; i<zenkit::DamageType::NUM; ++i)
    if(mask[i])
      ++nTypes;
  if(nTypes>0) {
    const int32_t share = int32_t(damage)/nTypes;
    int32_t total = 0;
    for(size_t i=0; i<zenkit::DamageType::NUM; ++i) {
      if(!mask[i] || hnpc.protection[i]<0)
        continue;
      total += std::max(share-hnpc.protection[i],0);
      }
    npc->changeAttribute(ATR_HITPOINTS,-total,false);
    }
  }

void TouchDamage::tick(uint64_t dt) {
  AbstractTrigger::tick(dt);

  if(world.tickCount()<=repeatTimeout) {
    if(intersections().empty())
      disableTicks();
    return;
    }

  for(auto npc:intersections())
    applyDamage(*npc);

  // NOTE: in original-game zCTouchDamage::OnTimer (Gothic2.exe 0x00615c70) the shared repeat is
  // re-armed only when repeatDelaySec>0; with repeatDelaySec==0 damage is dealt once per entry.
  if(repeatDelaySec>0)
    repeatTimeout = world.tickCount() + uint64_t(repeatDelaySec*1000);
  else
    repeatTimeout = std::numeric_limits<uint64_t>::max();

  if(intersections().empty())
    disableTicks();
  }
```

Rationale for faithfulness: the first occupant's entry hit and timer arming both happen in
`onIntersect` (matching `OnTouch`); subsequent entrants get their own entry hit without
disturbing the shared `repeatTimeout` (matching per-vob `OnTouch`); `tick` re-hits all current
occupants on the shared cadence (matching `OnTimer`). No double-hit on the first frame because
`repeatTimeout` is armed before `tick` runs and `tick` early-returns until it expires.
