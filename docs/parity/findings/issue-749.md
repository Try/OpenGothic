# Issue #749 — G2: Bartok can't hit anything with bow

- Category: NPC combat / ballistics (mod)
- Disposition: **DEFER** (NPC AI ballistics behavior; needs runtime reproduction/tuning. Labeled `mod`, G2 scriptpatch.)

## Problem
NPC Bartok, hunting with a bow (G2 + G2a_NR_ScriptPatch_v30), never hits a
shadowbeast the player can hit. Suggests NPC bow aim/ballistics or hit-chance
diverges.

## OG files
- `game/world/objects/npc.cpp` — `Npc::shootBow()` (lines 4081+): spawns the
  bullet via `owner.shootBullet(...)`, sets damage, and sets **hit chance** from
  `hnpc->hitchance[TALENT_BOW]/100` for G2 (lines 4116-4119).
- `game/world/world.cpp` — `World::shootBullet(itm,npc,target,inter)`
  (lines 673-706): computes the launch direction with a single-step gravity
  arc: `t = lxz/bulletSpeed; dir/=t; dir.y += 0.5*gravity*t;` aiming at
  `target->collosionCenter()`.
- `game/world/objects/npc.cpp:1701-1705` — AI tick path that calls
  `shootBow()` / `aimBow()`.

## Suspected divergence (to confirm at runtime)
Two independent candidates, both behavioral:
1. **Ballistic arc approximation.** `World::shootBullet` solves the vertical
   lead with one non-iterated step using horizontal distance `lxz` only. For a
   low/short shot at a shadowbeast this is usually fine, but if Bartok's
   `collosionCenter` aim point or the arc is biased, arrows consistently pass
   over/under. Player shots aim through the camera ray (different path:
   `playercontrol.cpp:725-752` + `shootBow(focus)`), which is why the player can
   hit while the NPC cannot.
2. **Hit-chance gate.** For G2, NPC bow accuracy is `hnpc->hitchance[TALENT_BOW]`.
   If the scriptpatch sets Bartok's bow hitchance low (or OG reads the wrong
   talent index), rolls may convert almost every arrow to a miss in
   `Bullet`/`takeDamage` resolution.

## Why DEFER
- Requires reproducing the exact scene (Bartok + shadowbeast, scriptpatch v30)
  and instrumenting `shootBullet` direction vs. actual target position and the
  hitchance roll outcome. Cannot be confirmed surgically from static reading.
- Mod-specific (`mod` label): the scriptpatch may itself set Bartok's talents,
  so the fix may be tuning OG's NPC-bullet aim/hitchance to match the original
  engine's NPC ranged solver, or confirming it is mod data.

## Original-behavior note (if pursued)
Compare against the original NPC ranged-attack solver (oCAIHuman / projectile
launch) in Gothic2.exe to verify the gravity-lead and hitchance application; do
not assume the single-step arc matches. Cite the original projectile fn when a
concrete divergence is found.

## Recommendation
Keep as DEFER/`help wanted`. First runtime step: log launch `dir`, target
center, and the hitchance roll in `World::shootBullet`/`Bullet` resolution for
Bartok's shots and compare to a player shot at the same shadowbeast.
