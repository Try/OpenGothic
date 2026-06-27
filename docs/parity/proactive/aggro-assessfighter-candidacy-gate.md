# PERC_ASSESSFIGHTER candidacy gate: exact predicate is "non-hostile + weapon drawn"

Confidence: High (predicate); the omission itself is also tracked in
`aistate-perc-fighter-item-missing.md` — this doc corrects that doc's guessed predicate.

## Original function + address (prose only)
`oCNpc::PerceptionCheck` @ 0x0075dd30 builds one vob list per perception tick and, for
each alive+conscious NPC it can sense, classifies the candidate into exactly one of two
mutually-exclusive "live target" buckets, then dispatches them in the NPC's own
perception-slot order at the tail switch (case 2 -> `oCNpc::AssessEnemy_S`, case 3 ->
`oCNpc::AssessFighter_S`):

- The branch computes the attitude toward the candidate with the same logic as
  `oCNpc::GetAttitude` @ 0x0072fab0 (inlined): for the player it honours the temp
  attitude field (oCNpc+0x7e8) when it differs from perm (oCNpc+0x7e4), otherwise perm;
  for a non-player it is the pure guild-matrix value. (This is NOT `GetPermAttitude`
  @ 0x0072fb30, which ignores temp — so OpenGothic's use of `personAttitude` for enemy
  candidacy is already correct.)
- If that attitude == 0 (`ATT_HOSTILE`) the candidate becomes the **AssessEnemy** target.
- Otherwise (any non-hostile attitude) the candidate becomes the **AssessFighter**
  target *only if* its weapon mode (oCNpc+0x250) is non-zero, i.e. it has a weapon
  drawn. The +0x250 field is the weapon/fight mode read by `oCNpc::GetWeaponMode`
  @ 0x00738c40, which clamps it to [0,7]; `PerceptionCheck` performs the identical
  [0,7] clamp before testing `!= 0`. WeaponState 0 = `NoWeapon`.

So `AssessFighter` is the engine's "a non-enemy near me has bared a weapon" reaction —
the player (when non-hostile) drawing a weapon in front of a guard/monster that has
`Npc_PercEnable(PERC_ASSESSFIGHTER, ...)` is a fighter candidate (and simultaneously an
AssessPlayer candidate; the player branch does not early-out of the attitude/weapon
test). This is the engine half of the draw-weapon-as-warning / "you've drawn steel"
threat reaction.

## OpenGothic file:line
`game/world/objects/npc.cpp:4461-4477` — `Npc::perceptionProcess(Npc& pl)` periodic
sweep. It handles only `PERC_ASSESSPLAYER`, `PERC_ASSESSENEMY` (via
`updateNearestEnemy`, line 4461) and `PERC_ASSESSBODY` (via `updateNearestBody`, line
4471). `PERC_ASSESSFIGHTER` (=3) is never raised anywhere (grep: appears only in
`game/game/constants.h:411`). There is no `updateNearestFighter`.

## Divergence
NPCs that enable `PERC_ASSESSFIGHTER` never run their `AssessFighter` script when a
non-hostile actor (notably the player) draws a weapon nearby, because OpenGothic's
periodic perception loop has no fighter slot at all.

The pre-existing analysis in `aistate-perc-fighter-item-missing.md` reaches the same
omission but proposes the wrong candidate predicate — it suggests
"`weaponState()!=WeaponState::NoWeapon` (or `bodyState()==BS_RUN` toward a target)" and
omits the attitude gate. The decompile shows the predicate is precisely:
**alive + conscious + sensable + NOT hostile (`!isEnemy`) + `weaponState()!=NoWeapon`**,
with no `bodyState`/`BS_RUN` component, and hostile actors deliberately excluded
(they are AssessEnemy, never AssessFighter).

## Proposed patch
DEFERRED (partial) — requires a new helper, not a single-line flip; flagged here so the
companion doc's predicate is corrected before either is implemented. The faithful shape:

Add a helper mirroring `updateNearestEnemy` but with the verified predicate, then a
dispatch block after the body block at `game/world/objects/npc.cpp:4477`.

Helper (grep-verified symbols: `isEnemy`, `isDown`, `weaponState`,
`WeaponState::NoWeapon`, `detectNpcNear`, `canSenseNpc`, `qDistTo` all exist):

```
Npc* Npc::updateNearestFighter() {
  if(aiPolicy!=NpcProcessPolicy::AiNormal)
    return nullptr;
  Npc*  ret  = nullptr;
  float dist = std::numeric_limits<float>::max();
  owner.detectNpcNear([this,&ret,&dist](Npc& n){
    // NOTE: in original-game oCNpc::PerceptionCheck @0x0075dd30 a non-hostile, alive,
    // weapon-drawn NPC (oCNpc+0x250 != 0, i.e. GetWeaponMode @0x00738c40 != NoWeapon)
    // is the AssessFighter candidate; hostile NPCs go to AssessEnemy instead.
    if(&n==this || n.isDown() || isEnemy(n) || n.weaponState()==WeaponState::NoWeapon)
      return;
    float d = qDistTo(n);
    if(d<dist && canSenseNpc(n,true)!=SensesBit::SENSE_NONE) {
      ret = &n; dist = d;
      }
    });
  return ret;
  }
```

Dispatch (insert after the `PERC_ASSESSBODY` block, current line 4477):

OLD:
```
  Npc* body=hasPerc(PERC_ASSESSBODY) ? updateNearestBody() : nullptr;
  if(body!=nullptr){
    float dist=qDistTo(*body);
    if(perceptionProcess(*body,nullptr,dist,PERC_ASSESSBODY)) {
      ret = true;
      }
    }
```
NEW:
```
  Npc* body=hasPerc(PERC_ASSESSBODY) ? updateNearestBody() : nullptr;
  if(body!=nullptr){
    float dist=qDistTo(*body);
    if(perceptionProcess(*body,nullptr,dist,PERC_ASSESSBODY)) {
      ret = true;
      }
    }

  // NOTE: in original-game oCNpc::PerceptionCheck @0x0075dd30 the same periodic scan
  // also dispatches AssessFighter_S for the nearest sensed non-hostile, weapon-drawn NPC.
  Npc* fighter=hasPerc(PERC_ASSESSFIGHTER) ? updateNearestFighter() : nullptr;
  if(fighter!=nullptr){
    float dist=qDistTo(*fighter);
    if(perceptionProcess(*fighter,nullptr,dist,PERC_ASSESSFIGHTER))
      ret = true;
    }
```

Reason DEFERRED rather than applied: adds a new active perception behavior (regression
surface) and a new method; should be built+playtested. The PERC_ASSESSITEM half of the
companion doc is a separate concern and not addressed here.
