# Npc::canSenseNpc range gate uses exact Euclidean, but the original oCNpc::CanSense uses the octagonal GetDistanceToVobApprox

**Confidence:** High

## Original function + address (prose)

`oCNpc::CanSense` (`Gothic2.exe @0x00740740`) is the narrow-phase perception gate. Its
smell branch is taken when the senses bitfield (oCNpc+0x280) has bit `0x4` set; it accepts
the target when `zCVob::GetDistanceToVobApprox(this, target) <= senses_range` (the per-NPC
range field at oCNpc+0x284), with no line-of-sight test. Its sight branch (`oCNpc::CanSee`
@0x00741c10) bounds visibility against the *same* `senses_range` using the same octagonal
fast-distance approximation.

`zCVob::GetDistanceToVobApprox` (`@0x0061b970`, decompile-verified) reads the two vobs' world
positions from their trafo translation columns (offsets 0x48/0x58/0x68, stride 0x10 — these
are *positions*, not a bounding box) and returns the octagonal approximation
`dApprox = 0.375*(|dx|+|dy|+|dz|) + 0.5625*max(|dx|,|dy|,|dz|)`
(equivalently `0.375*(sum of two smaller axes) + 0.9375*max`). This underestimates the true
Euclidean length by up to ~7.2% along a 2D diagonal and ~2.6% along a 3D diagonal, and is
exact only on axis-aligned offsets. Because `dApprox <= range` while OpenGothic tests
`euclid <= range`, the original's accepted set is *larger*: the original senses/aggros targets
from a slightly farther true distance along diagonals than OpenGothic does.

This is the identical metric already accepted for the `Npc_GetDistToPlayer` fix
(`senses-getdisttoplayer-approx.md`, GetDistToPlayer @0x006f3180 → GetDistanceToVobApprox).

## OG file:line

`game/world/objects/npc.cpp:5122-5125` — `Npc::canSenseNpc(const Tempest::Vec3 pos, bool, bool, float)`:

```cpp
const float range = float(hnpc->senses_range)+extRange;
if(qDistTo(pos)>range*range)
  return SensesBit::SENSE_NONE;
```

`qDistTo()` (npc.cpp:743) returns `(pos-centerPosition()).quadLength()` — exact squared
Euclidean. This single gate governs both the SMELL result and (after the raycast) the SEE
result, so fixing it matches both original branches' radius at once.

## Divergence

OpenGothic compares exact Euclidean distance against `senses_range`; the original compares the
octagonal `GetDistanceToVobApprox` against `senses_range`. OpenGothic's effective sense/aggro
radius is therefore ~3-7% smaller along diagonal approach directions for every perception that
flows through `canSenseNpc` (enemy detection, ASSESSPLAYER/ENEMY, monster aggro, flee sensing).

Note: the existing `panic-sense-distance-metric.md` saw this code but DEFERRED it on the
mistaken premise that `GetDistanceToVobApprox` is "bounding-box aware, edge-to-edge"; the
decompile shows it is a pure vob-origin position metric, so the real (and surgical) divergence
is the octagonal-vs-Euclidean metric, not a bbox concern.

## Proposed patch (OLD/NEW)

OLD (`game/world/objects/npc.cpp`, in `Npc::canSenseNpc(const Tempest::Vec3 pos, ...)`):
```cpp
  const float range = float(hnpc->senses_range)+extRange;
  if(qDistTo(pos)>range*range)
    return SensesBit::SENSE_NONE;
```

NEW:
```cpp
  const float range = float(hnpc->senses_range)+extRange;
  // NOTE: in original-game oCNpc::CanSense @0x00740740 the smell-branch range gate compares
  // zCVob::GetDistanceToVobApprox (@0x0061b970) <= senses_range, and the sight branch
  // (oCNpc::CanSee @0x00741c10) bounds sight with the same octagonal fast-distance metric.
  // GetDistanceToVobApprox is NOT exact Euclidean: it returns
  //   0.375*(|dx|+|dy|+|dz|) + 0.5625*max(|dx|,|dy|,|dz|),
  // which underestimates the true length by up to ~7% along diagonals. OpenGothic used exact
  // qDistTo(), which shrinks the effective sense radius along diagonal approaches; recompute
  // with the original's octagonal metric. (Position basis center-vs-vob-origin is a separate
  // pre-existing concern and is left unchanged.)
  const auto  d  = pos - centerPosition();
  const float ax = std::abs(d.x), ay = std::abs(d.y), az = std::abs(d.z);
  const float mx = std::max(ax,std::max(ay,az));
  const float dApprox = 0.375f*(ax+ay+az) + 0.5625f*mx;
  if(dApprox>range)
    return SensesBit::SENSE_NONE;
```

`centerPosition()`, `std::abs`, and `std::max` are all already used in npc.cpp; build-safe.
