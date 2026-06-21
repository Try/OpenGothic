# Periodic perception loop omits PERC_ASSESSFIGHTER and PERC_ASSESSITEM

Confidence: Medium

## Original fn + addr
`oCNpc::PerceptionCheck` (0x0075dd30).

Each tick the original advances a fractional accumulator and, when it crosses the
perception interval, builds one vob list (`CreateVobList`) and scans it once. It
gathers, per scan, the nearest sensed candidate for each *active* perception slot:
the player (slot 1), an enemy (slot 2), a fighter (slot 3 — another NPC currently
in a fight/aware state), a dead body (slot 4) and an item (slot 5). After the scan
it dispatches assessment callbacks in slot order via a switch: case 1 ->
`AssessPlayer_S`, case 2 -> `AssessEnemy_S`, case 3 -> `AssessFighter_S`, case 4 ->
`AssessBody_S`, case 5 -> `AssessItem_S`. So an NPC with `Npc_PercEnable(PERC_ASSESSFIGHTER)`
or `Npc_PercEnable(PERC_ASSESSITEM)` actively scans for and reacts to those targets
on the normal perception cadence.

## OG file:line
`game/world/objects/npc.cpp:4212-4255` (`Npc::perceptionProcess(Npc& pl)`), driven by
`game/world/worldobjects.cpp:250-253`.

The periodic loop only handles three slots:
`PERC_ASSESSPLAYER` (line 4228), `PERC_ASSESSENEMY` (4234) via `updateNearestEnemy`,
and `PERC_ASSESSBODY` (4244) via `updateNearestBody`. There is no `updateNearestFighter`
and no `updateNearestItem`, and neither `PERC_ASSESSFIGHTER` (=3) nor
`PERC_ASSESSITEM` (=5) is ever raised from a periodic scan anywhere in the codebase
(grep: both constants appear only in `game/game/constants.h`).

## Divergence
The original raises PERC_ASSESSFIGHTER and PERC_ASSESSITEM periodically; OG never
raises them at all. Gameplay-different: monsters/NPCs that script
`Npc_PercEnable(PERC_ASSESSITEM,...)` (scavengers and lurkers homing on dropped
meat/items, NPCs picking up nearby loot) and bystanders that enable
PERC_ASSESSFIGHTER to react to a fight starting nearby never trigger those reactions
in OpenGothic.

## Proposed patch
A faithful fix needs two small helpers mirroring `updateNearestBody` plus two extra
blocks in the periodic loop. (Not a single-line value flip — flagged "partial".)

`game/world/objects/npc.cpp` — add after the body block (around line 4250):

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

  // NOTE: in original-game oCNpc::PerceptionCheck (0x0075dd30) the same periodic
  // scan also dispatches AssessFighter_S (PERC_ASSESSFIGHTER) and AssessItem_S
  // (PERC_ASSESSITEM). Mirror those two missing slots here.
  Npc* fighter=hasPerc(PERC_ASSESSFIGHTER) ? updateNearestFighter() : nullptr;
  if(fighter!=nullptr){
    float dist=qDistTo(*fighter);
    if(perceptionProcess(*fighter,nullptr,dist,PERC_ASSESSFIGHTER))
      ret = true;
    }

  Item* itm=hasPerc(PERC_ASSESSITEM) ? updateNearestWldItem() : nullptr;
  if(itm!=nullptr){
    float dist=qDistTo(*itm);
    if(perceptionProcess(*itm,dist,PERC_ASSESSITEM))
      ret = true;
    }
```

`updateNearestFighter()` mirrors `updateNearestBody` but selects the nearest sensed
NPC with `weaponState()!=WeaponState::NoWeapon` (or `bodyState()==BS_RUN` toward a
target). `updateNearestWldItem()` scans `owner.detectItem`-style nearby world items.
Both require small new methods plus an `Item`-overload of `perceptionProcess`; without
them the two perceptions stay silent.
