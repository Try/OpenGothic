# Combat-noise / sound-perception recipient gate is a sphere in OpenGothic, an axis-aligned cube in the original

**Confidence:** Medium-High (original behaviour is fully decompile-verified; OpenGothic behaviour is verified in source. The only soft edges are the point-in-cube vs bbox-overlap approximation and the downstream-gate handling, both noted below.)

## Original function + address (prose only)

In `Gothic2.exe`, every *passive* and *sound* perception broadcast selects its recipients with an
**axis-aligned bounding-box (cube)** query, with **no** spherical narrow-phase:

- `oCNpc::CreateSoundPerception` (0x0075bb70) — the broadcaster for the sound perceptions
  `PERC_ASSESSQUIETSOUND` / `PERC_ASSESSLOUDSOUND` / `PERC_ASSESSFIGHTSOUND`. It reads the global
  `percRange[perc]` (data at 0x00ab24d8, indexed 0..32), builds a `zTBBox3D` of half-extent
  `percRange[perc]` centred on the sound position, calls `zCBspBase::CollectVobsInBBox3D`, and for
  every collected NPC that has the perception registered and is alive/awake it calls
  `oCNpc_States::StartAIState(perceptionFunc, ...)` directly. There is no `GetDistanceToVob` /
  `IsInPerceptionRange` distance recheck.
- `oCNpc::CreatePassivePerception` (0x0075b270) — the broadcaster for the non-sound passive
  perceptions (e.g. `PERC_ASSESSOTHERSDAMAGE`, `PERC_ASSESSMURDER`, ...). Same shape: it calls
  `oCNpc::CreateVobList` (0x0075d730), which builds the same cube (`pos ± percRange[perc]`) and runs
  `CollectVobsInBBox3D`, then `StartAIState` per recipient. Again no spherical recheck.

(For contrast, the *active* perceptions go through `oCNpc::PerceptionCheck` (0x0075dd30), which
broad-phases on the NPC's own `senses_range` and then narrow-phases each candidate through
`oCNpc::IsInPerceptionRange` (0x0075e460) — a euclidean/`GetDistanceToVob` sphere vs `percRange[perc]`.
So a sphere is correct for the *active* path only.)

Ground truth on the range values: the global `percRange[]` is zero-initialised BSS and is populated
entirely from script via the `Perc_SetRange` external (engine `oCNpc::SetPerceptionRange`
0x0075e440). `GOTHIC.DAT` contains a `Perc_SetRange` table (one `PushVar(PERC_*) ; PushInt(range) ;
call Perc_SetRange` per type, ~17 entries) that sets *every* perception range (sound types up to
9999, fight/assess types ~1880-1890). Consequently the per-NPC `senses_range` fallback in
OpenGothic's `PerDist::at` is dormant for stock G2 — the divergence is the recipient-gate **shape**
(cube vs sphere), not the radius.

## OpenGothic file:line

`game/world/worldobjects.cpp:949-977` — `WorldObjects::passivePerceptionProcess`, specifically the
gate at lines 959-963 and the downstream call at line 976:

```cpp
const float distance = npc.qDistTo(msg.pos);
const float range    = float(owner.script().percRanges().at(PercType(msg.what), npc.handle().senses_range));

if(distance > range*range)        // <-- SPHERE
  return;
...
npc.perceptionProcess(*msg.other,msg.victim,distance,PercType(msg.what));
```

This function is the single gate for both the queued `sndPerc` broadcasts (from
`WorldObjects::sendPassivePerc`, used by `PERC_ASSESSFIGHTSOUND`, `PERC_ASSESSOTHERSDAMAGE`, ...) and
the immediate path (`WorldObjects::sendImmediatePerc`). So it stands in for *both* original
broadcasters above, and both of those use a cube.

## Divergence

OpenGothic gates recipients with a sphere of radius `range` (`distance > range*range`), whereas the
original gates with an axis-aligned cube of half-extent `range`. An NPC that lies inside the cube but
outside the sphere — i.e. up to `range*sqrt(3)` (~1.73x) away along a diagonal — is alerted in the
original but **dropped** in OpenGothic. For combat noise this means OpenGothic under-recruits nearby
allies/guards from a fight at the corners of the perception volume (e.g. a fight-sound radius of
~1880 reaches ~3250 on a diagonal in the original, ~1880 in OpenGothic).

Secondary, redundant gate: `Npc::perceptionProcess` (`game/world/objects/npc.cpp:4485-4489`)
re-applies the *same* spherical `quadDist>r` check. Today it is a harmless no-op for the passive path
(the caller already passed a stricter sphere), but it is shared with the active path where the sphere
is correct — so the fix must neutralise it for the passive path without touching the active path.

## Proposed patch

Replace the spherical gate with an axis-aligned box gate (mirroring `CollectVobsInBBox3D`), and pass
`0` as the already-gated distance so the shared `perceptionProcess` sphere does not re-reject the
newly-admitted corner recipients. All symbols grep-verified: `npc.centerPosition()`
(`npc.h:125`, used by `qDistTo`), `Tempest::Vec3` `.x/.y/.z`, `percRanges().at(...)`
(`gamescript.h:151`), `Npc::perceptionProcess` (`npc.cpp:4479`).

OLD (`game/world/worldobjects.cpp:959-963`):
```cpp
  const float distance = npc.qDistTo(msg.pos);
  const float range    = float(owner.script().percRanges().at(PercType(msg.what), npc.handle().senses_range));

  if(distance > range*range)
    return;
```
NEW:
```cpp
  // NOTE: in original-game oCNpc::CreateSoundPerception @0x0075bb70 and
  // oCNpc::CreatePassivePerception @0x0075b270 recipients are selected by
  // zCBspBase::CollectVobsInBBox3D over an axis-aligned cube of half-extent percRange[perc]
  // (oCNpc::CreateVobList @0x0075d730), with no spherical narrow-phase. OpenGothic used a sphere
  // (distance>range^2), dropping NPCs in the cube corners (out to ~1.73*range on a diagonal), so
  // fewer allies were alerted by combat noise / fight-sound.
  const float range = float(owner.script().percRanges().at(PercType(msg.what), npc.handle().senses_range));
  const auto  dp    = npc.centerPosition() - msg.pos;
  const float r2    = range*range;
  if(dp.x*dp.x>r2 || dp.y*dp.y>r2 || dp.z*dp.z>r2)
    return;
```

OLD (`game/world/worldobjects.cpp:976`):
```cpp
  npc.perceptionProcess(*msg.other,msg.victim,distance,PercType(msg.what));
```
NEW:
```cpp
  // range already gated as an axis-aligned box above; pass 0 so perceptionProcess does not
  // re-apply its spherical range gate (that gate is for the active-perception path).
  npc.perceptionProcess(*msg.other,msg.victim,0,PercType(msg.what));
```

Notes on residual approximation (do not block the fix, but disclose): the original cube test is a
vob-bbox-overlap, not a point-in-cube; this patch uses the NPC center point (consistent with the
existing `qDistTo` semantics), which is the standard OpenGothic idiom for mirroring `CollectVobsInBBox3D`.
