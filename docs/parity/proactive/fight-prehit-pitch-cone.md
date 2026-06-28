# fight: enemy prehit/storm reaction missing the ±50° vertical (pitch) front-cone gate

**Confidence:** Medium

## Original fn + address

`oCNpc::FindNextFightAction` (Gothic2.exe `0x0067d680`, melee branch where
`GetWeaponMode() < 5`). Before the move-selection switch the function builds the
front-cone predicate `local_90` from `oCNpc::GetAngles` (`0x006812b0`), whose
out-params are `(yaw, pitch)`:

- it sets `local_90 = 1` **only** when `ABS(yaw) < 90.0` (`0x42b40000`) **and**
  `ABS(pitch) < 50.0` (`0x42480000`); otherwise `local_90 = 0`.

`local_90` is the gate that the engine ANDs into both **case 0** (`enemy_prehit`,
target situation `PREHIT` = `0xc`) and **case 1** (`enemy_stormprehit`, target
situation `STORMPREHIT` = `0x10`). So the original requires the attacker to be
inside a cone that is **±90° horizontally AND ±50° vertically** before the NPC is
allowed to play its scripted parry/dodge reaction. The vertical limit is applied
*in addition to* the same-height test already folded into the range predicate
`IsInFightRange(target, me)` (`local_8c`).

## OG file:line

`game/game/fightalgo.cpp:55` — the `enemy_stormprehit` / `enemy_prehit` gate:

```cpp
if(isInWRange(tg,npc,owner) && isInFocusAngle(tg,npc) && isInFocusAngle(npc,tg,90)){
```

`isInFocusAngle(npc,tg,90)` reproduces only the **horizontal** ±90° cone.
`FightAlgo::angleTest` (`fightalgo.cpp:368`) is deliberately 2-D
(`atan2(dpos.z, dpos.x)`), so the original's ±50° **pitch** limit is not
reproduced anywhere in this gate.

## Divergence

OpenGothic gates the prehit/storm reaction on horizontal yaw (±90°) only. The
original additionally requires the relative vertical angle to the attacker to be
within ±50°. OpenGothic's `fightSameHeight` (used inside `qDistTo` →
`isInWRange`) is a bbox-overlap test, not the GetAngles pitch test, and the
original applies its same-height check *and* the ±50° pitch gate together — so
the pitch limit is an extra constraint that OpenGothic drops. Practical effect:
on stairs/ledges where the attacker is steeply above or below (relative pitch
>50°) but the collision bboxes still overlap vertically, OpenGothic plays the
prehit/storm reaction where the original would suppress it and let the NPC fall
through to the normal in-range tables. Edge-case, but a concrete constant
(50.0) that is currently absent.

## Proposed patch (NOT applied)

Add a vertical-angle helper analogous to `angleTest`, used only in the
prehit/storm gate. For a yaw-only NPC the model AT-vector is horizontal, so the
relative pitch reduces to the elevation of the attacker:
`atan2(|dy|, sqrt(dx²+dz²))`.

OLD (`fightalgo.cpp:55`):
```cpp
  if(isInWRange(tg,npc,owner) && isInFocusAngle(tg,npc) && isInFocusAngle(npc,tg,90)){
```

NEW:
```cpp
  // NOTE: in original-game oCNpc::FindNextFightAction (Gothic2.exe 0x0067d680) the prehit/storm
  // front-cone predicate (built from oCNpc::GetAngles @0x006812b0) requires BOTH |yaw|<90 deg
  // AND |pitch|<50 deg; the ±50-deg vertical limit is applied on top of the same-height test
  // already inside IsInFightRange. OpenGothic reproduced only the horizontal ±90 cone, so an
  // attacker at a steep vertical angle (steep stairs/ledge) but still bbox-height-overlapping
  // wrongly triggered the parry/dodge reaction.
  if(isInWRange(tg,npc,owner) && isInFocusAngle(tg,npc) && isInFocusAngle(npc,tg,90) &&
     isInPitchAngle(npc,tg,50)){
```

with a new static helper (mirroring `angleTest`, all required symbols exist:
`collosionCenter()`):
```cpp
bool FightAlgo::isInPitchAngle(const Npc& npc, const Npc& tg, float ang) {
  const auto  dpos  = tg.collosionCenter() - npc.collosionCenter();
  const float horiz = std::sqrt(dpos.x*dpos.x + dpos.z*dpos.z);
  const float pitch = std::atan2(std::fabs(dpos.y), horiz); // NPCs are yaw-only: AT-vector is horizontal
  return pitch < float(ang*M_PI/180.0);
  }
```

### Caveat / why Medium and not High
`fightSameHeight` already excludes most steep-vertical configurations, so the
observable surface is small and the two gates overlap. If maintainers consider
the bbox same-height test a sufficient stand-in for the pitch cone this is WAI;
the divergence is real against the binary but low-impact, hence Medium.
