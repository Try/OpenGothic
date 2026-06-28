# FLY (knockback) throwback is applied to swimming/diving victims (missing water-level gate)

**Confidence:** High (the original's water-level gate on the throwback branch is explicit and
unambiguous; OG omits it. The OG team's own `dontKill` line in the same function already maps the
identical original predicate to `!isSwim() && !isDive()`, so the fix idiom is established).

## Original function + address (prose)

`oCNpc::OnDamage_Anim` (Gothic2.exe `0x00675bd0`), reached from the weapon/spell damage pipeline
`oCNpc::OnDamage` (`0x006660e0`) -> `oCNpc::OnDamage_Hit` (`0x00666610`) -> `OnDamage_Anim`.

Inside `OnDamage_Anim` the knockback ("fly") throwback is dispatched through
`oCAIHuman::StartFlyDamage` (`0x0069d940`, called at `0x00679737`). The branch that reaches
`StartFlyDamage` is guarded by a three-part condition. Stripped of the heavy debug-string noise, the
engine takes the throwback **only** when all of the following hold:

    applyVictimState != 0
    && oCAniCtrl_Human::GetWaterLevel(anictrl) == 0          // @0x006b89d0: 0=land, 1=swim, 2=dive
    && oCNpc::GetBodyState(this)            != BS_LIE (0xc)

i.e. the original guard is literally `if (applyVictimState==0 || GetWaterLevel()!=0 || GetBodyState()==0xc) { no throwback } else { StartFlyDamage(...) }`.

So a victim that is **swimming or diving** (`GetWaterLevel()` returns 1 or 2) is never knocked back
by a FLY-type hit, even when the blow carries victim state and positive knockback force. `GetWaterLevel`
is the same accessor OG already cites elsewhere in `npc.cpp` (see the comment at npc.cpp:3672 referencing
`oCAniCtrl_Human::GetWaterLevel @0x006b89d0`, where level 1 = swim and level 2 = dive).

## OpenGothic file:line

`game/world/objects/npc.cpp:2240-2243` (in `Npc::takeDamage(Npc&, const Bullet*, CollideMask, int32_t, bool)`):

```cpp
  if(hitResult.hasHit && (damageType & (1<<zenkit::DamageType::FLY)) &&
     (bMask&(COLL_APPLYVICTIMSTATE|COLL_DOEVERYTHING))) {
    mvAlgo.accessDamFly(x-other.x, z-other.z, lastHitType);
    }
```

## Divergence

A prior fix (`spellstun-fly-throwback-victimstate-gate.md`) restored the `applyVictimState`
(`COLL_APPLYVICTIMSTATE|COLL_DOEVERYTHING`) half of the original guard, but the **water-level half was
left out** (that doc only describes it in prose and explicitly defers the rest of the branch). As a
result OG still calls `mvAlgo.accessDamFly(...)` for a FLY hit landed on a **swimming or diving** NPC,
whereas stock Gothic II suppresses the throwback entirely in water. Observable effect: in OpenGothic a
knockback spell / FLY weapon hit launches a swimming/diving target (water-level 1 or 2) into a fly
trajectory; in the retail game the same hit deals its damage but produces no throwback while the victim
is in the water.

The `GetBodyState()==BS_LIE(0xc)` half of the original guard is already effectively covered in OG: a
lying/unconscious victim is caught by the earlier `isDown()` early-return at npc.cpp:2206 before the
throwback is reached, so the only un-mirrored sub-gate is the water-level one.

`GetWaterLevel()!=0` (swim level 1 OR dive level 2) maps to `isSwim() || isDive()` in OG. This mapping is
not guessed: the same function's `dontKill` computation at npc.cpp:2176 already uses
`(!isSwim() && !isDive())` to reproduce the original's `oCAniCtrl_Human::IsInWater` / water-level guard
(see the NOTE at npc.cpp:2171-2175 explaining that OG's mutually-exclusive MoveAlgo states require
checking both `isSwim()` and `isDive()` to cover water levels 1 and 2).

## Proposed patch

`game/world/objects/npc.cpp`

OLD (2240-2243):
```cpp
  if(hitResult.hasHit && (damageType & (1<<zenkit::DamageType::FLY)) &&
     (bMask&(COLL_APPLYVICTIMSTATE|COLL_DOEVERYTHING))) {
    mvAlgo.accessDamFly(x-other.x, z-other.z, lastHitType);
    }
```

NEW:
```cpp
  // NOTE: in original-game oCNpc::OnDamage_Anim (Gothic2.exe 0x00675bd0) the FLY throwback branch
  // (oCAIHuman::StartFlyDamage @0x0069d940, @0x00679737) is additionally gated on
  // oCAniCtrl_Human::GetWaterLevel(anictrl)==0 (@0x006b89d0): a swimming (level 1) or diving
  // (level 2) victim is never knocked back. OG mirrors that water-level guard the same way the
  // dontKill line above does -- !isSwim() && !isDive() covers water levels 1 and 2. (The
  // GetBodyState()==BS_LIE(0xc) half of the original guard is already covered by the isDown()
  // early-return earlier in this function.)
  if(hitResult.hasHit && (damageType & (1<<zenkit::DamageType::FLY)) &&
     (bMask&(COLL_APPLYVICTIMSTATE|COLL_DOEVERYTHING)) &&
     !isSwim() && !isDive()) {
    mvAlgo.accessDamFly(x-other.x, z-other.z, lastHitType);
    }
```

Grep-verified OG symbols: `isSwim()` / `isDive()` (npc.h:196,198; npc.cpp:1092,1100), `mvAlgo.accessDamFly`
(movealgo.cpp:500, called npc.cpp:2242), `hitResult.hasHit`, `damageType`, `bMask`,
`COLL_APPLYVICTIMSTATE`/`COLL_DOEVERYTHING` (constants.h).
