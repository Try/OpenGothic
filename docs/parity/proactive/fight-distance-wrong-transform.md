# fightDistanceTo projects target root-bone with the attacker's transform

**Confidence:** Medium

## Original function + address
`oCNpc::IsInFightRange` (0x0067cb60) / `IsInDoubleFightRange` (0x0067c9a0),
`oNpc_Fight.cpp`. The original obtains each NPC's world position via that NPC's own
`zMAT4::GetTranslation` on its own world matrix, then forms the center-to-center vector.
Each endpoint is expressed in its *own* object's frame; there is no cross-application of
one NPC's transform to the other NPC's local data.

## OpenGothic location
`game/world/objects/npc.cpp`, `Npc::fightDistanceTo` (lines 746-762):

```
if(auto sk = visual.visualSkeleton()) {
  cen = sk->rootTr;
  transform().project(cen);          // line 752 - self's transform on self's rootTr (OK)
  }
cen += position();

if(auto sk = tg.visual.visualSkeleton()) {
  tgCen = sk->rootTr;
  transform().project(tgCen);        // line 758 - BUG: self's transform on TARGET's rootTr
  }
tgCen += tg.position();
```

## Divergence
Line 758 projects the **target's** root-bone translation `tgCen` using `transform()`
(== `this->transform()`, the attacker's world matrix) instead of `tg.transform()`. The
target's root offset is therefore rotated by the attacker's orientation rather than the
target's own orientation. `rootTr` is the skeleton root_translation (root-motion drift,
non-trivial in XZ during locomotion/attack animations: skeleton.cpp:44-45), so the
resulting fight distance is wrong by a direction-dependent amount whenever attacker and
target face different ways. This feeds every range predicate via `qDistTo`, slightly
mis-judging W/G/attack/closeup ranges in combat.

## Proposed patch
File: `game/world/objects/npc.cpp`

OLD:
```
  if(auto sk = tg.visual.visualSkeleton()) {
    tgCen = sk->rootTr;
    transform().project(tgCen);
    }
  tgCen += tg.position();
```
NEW:
```
  if(auto sk = tg.visual.visualSkeleton()) {
    tgCen = sk->rootTr;
    // NOTE: in original-game each NPC's position is taken from its OWN world matrix;
    // the target's root-bone must be rotated by the target's transform, not the attacker's.
    tg.transform().project(tgCen);
    }
  tgCen += tg.position();
```
