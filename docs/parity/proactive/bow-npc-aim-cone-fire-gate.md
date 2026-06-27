# NPC bow/crossbow attack fires without the 5-degree aim-cone gate and 3 s aim timeout

**Confidence:** Medium (divergence proven against the decompiler; fix DEFERRED — see reason)

## Original function + address

`oCNpc::EV_AttackBow` (Gothic2.exe `0x0067f7e0`) is the per-tick handler that decides
whether a ranged-fighting NPC actually *releases* an arrow/bolt during a fight-AI attack
move. In prose, each tick it:

1. computes the yaw/pitch to its focus/enemy vob (`oCNpc::GetAngles`),
2. takes the absolute integer yaw and tests `abs(yaw) < 5` (degrees) **and**
   `oCNpc::FreeLineOfSight(target)`;
3. if **both** hold, it stops the turn anis and posts the actual shoot message
   (subtype 7, normal) — the arrow is loosed;
4. if not aligned, it instead *decrements an aim countdown* stored on the pending
   attack message (`msg.field[0x50] -= DAT_0099b3d8`, the per-frame time delta) and
   returns without firing — i.e. it keeps turning/aiming. Only when that countdown
   reaches `<= 0` does it post a forced shoot message (subtype 7, abort flag) and loose
   anyway.

The countdown's initial value is set when the ranged attack is queued in
`oCNpc::FightAttackBow` (Gothic2.exe `0x0067f700`), which writes `3000.0` (= `0x453b8000`,
3000 ms) into the attack-message timer field (`msg[0x14]` == byte offset `0x50`). So a stock
ranged NPC will turn-and-aim for up to **3 seconds** trying to get within a **5-degree**
yaw cone with clear line-of-sight, and only then (or on timeout) releases the shot.

## OpenGothic divergence

`game/world/objects/npc.cpp:1753-1760` (ranged branch of the fight-AI attack action):

```cpp
else if(ws==WeaponState::Bow || ws==WeaponState::CBow) {
  if(shootBow()) {
    fghAlgo.consumeAction();
    }
  else if(!implTurnToFai(*currentTarget,dt)) {
    aimBow();
    }
  }
```

`Npc::shootBow()` (`npc.cpp:4367`) is attempted **first** and returns `true` whenever the
body state is one of `BS_STAND/BS_AIMNEAR/BS_AIMFAR/BS_HIT` and ammo is present — it never
consults the yaw to `currentTarget`. Because it almost always succeeds, the `else if`
turn-to-align branch is effectively dead during an attack: the NPC looses the shot on the
first attack tick regardless of how far off its facing is (the fight-AI only required the
coarse 30-degree focus cone, `fightalgo.cpp:353`, to *select* the ranged table). The
original's tight 5-degree release cone and 3000 ms aim-then-fire countdown have no
counterpart. OpenGothic only models the line-of-sight half of the gate (the obstacle ray at
`npc.cpp:1722-1731`).

Net effect: OpenGothic ranged NPCs snap-fire the instant the attack move is queued instead
of visibly turning to line up the shot, changing archer firing cadence and removing the
characteristic "draw, track, loose" rhythm of the original. (The arrows still connect because
`World::shootBullet` at `world.cpp:675-683` aims the projectile directly at the target's
collision center, so the missing alignment is cadence/animation rather than accuracy.)

## Proposed patch — DEFERRED

A faithful fix needs to (a) gate `shootBow()` on a ~5-degree yaw cone to `currentTarget`
(the public helper `fghAlgo.isInFocusAngle(*this,*currentTarget,5.f)` already exists and is
grep-verified at `fightalgo.h:59`), turning via `implTurnToFai` otherwise, **and** (b)
reproduce the 3000 ms aim countdown so an NPC tracking a fast-strafing target still fires on
timeout. Sketch:

```cpp
// NOTE: in original-game oCNpc::EV_AttackBow @0x0067f7e0 the arrow is released only when
// |yaw-to-target| < 5deg AND line-of-sight is clear; otherwise the NPC keeps turning until
// a 3000ms aim countdown (seeded in oCNpc::FightAttackBow @0x0067f700) expires, then fires.
else if(ws==WeaponState::Bow || ws==WeaponState::CBow) {
  if(fghAlgo.isInFocusAngle(*this,*currentTarget,5.f) /* || aimTimeout */) {
    if(shootBow())
      fghAlgo.consumeAction();
    }
  else if(!implTurnToFai(*currentTarget,dt)) {
    aimBow();
    }
  }
```

Deferred because the surgical part (the `isInFocusAngle(...,5)` gate) alone, **without** the
3000 ms timeout state, would let a circling/strafing target keep a ranged NPC permanently
under 5 degrees of alignment and so never firing — a regression the original explicitly
avoids with its countdown. Adding that countdown means new per-attack timer state on the
fight path, which is beyond a one-line surgical change and needs runtime validation of
archer cadence. Constants to carry into any future fix: **5-degree** release cone,
**3000 ms** aim timeout.
