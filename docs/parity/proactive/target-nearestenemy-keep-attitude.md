# Cached nearestEnemy is retained without re-checking hostility

**Confidence:** Medium-High (clear logic asymmetry; one-line, strictly-more-faithful, build-verifiable fix)

## Original function + address (prose)

The engine has no persistent "nearest enemy" cache. Each perception cycle,
`oCNpc::PerceptionCheck` (Gothic2.exe @0x0075dd30) rebuilds the candidate vob
list from scratch (`CreateVobList` around `senses_range`) and re-classifies
*every* vob that cycle. An alive, conscious NPC vob (`hp >= 1` and not
`oCNpc_States::IsInState(-4)`, i.e. not unconscious) is selected as the
`PERC_ASSESSENEMY` candidate only when its *live* attitude is hostile:
`oCNpc::GetPermAttitude` (@0x0072fb30) / the guild-attitude lookup
(`oCGuilds::GetAttitude` @0x00700d40) must return `0` (ATT_HOSTILE). The
previously-assessed enemy enjoys no special status — it must pass the same
hostility test again, or it simply is not classified as an enemy that cycle.
(The script-side sticky path lives in `oCNpc::GetNextEnemy`/`SetEnemy`, which is
separate and already handled; this finding is about the engine perception's
`PERC_ASSESSENEMY` candidate, i.e. OpenGothic's `updateNearestEnemy`.)

## OG file:line

`/Users/admin/Downloads/opengothic/game/world/objects/npc.cpp:2363-2367`
(`Npc::updateNearestEnemy`, the "keep current nearestEnemy" branch).

## Divergence

`updateNearestEnemy` caches the last enemy in `nearestEnemy`. The fresh-scan
path (line 2370) correctly rejects non-hostile candidates:
`if(!isEnemy(n) || n.isDown() || &n==this) return;`. But the cache-retention
branch only re-checks liveness and senses, *not* attitude:

```cpp
if(nearestEnemy!=nullptr &&
   (!nearestEnemy->isDown() && canSenseNpc(*nearestEnemy,true)!=SensesBit::SENSE_NONE)) {
  ret  = nearestEnemy;
  dist = qDistTo(*ret);
  }
```

So once an NPC has been latched as `nearestEnemy`, a subsequent attitude flip to
non-hostile (e.g. `Npc_SetTempAttitude` calming a guard, the NPC/player becoming
a party member, or a guild-attitude change) does **not** drop it: as long as it
stays alive and inside senses range, and no *closer* hostile is found, it is
returned again and keeps firing `PERC_ASSESSENEMY` (B_AssessEnemy). The original
engine re-evaluates `GetPermAttitude` every cycle, so a now-friendly NPC is never
re-surfaced as the enemy candidate. The asymmetry (scan path checks `isEnemy`,
retention path does not) is the divergence; B_AssessEnemy often re-guards on
`Npc_GetAttitude` script-side, which is why the visible effect is usually limited
to spurious perception invocations rather than guaranteed aggro.

## Proposed patch

```cpp
// OLD
  if(nearestEnemy!=nullptr &&
     (!nearestEnemy->isDown() && canSenseNpc(*nearestEnemy,true)!=SensesBit::SENSE_NONE)) {
    ret  = nearestEnemy;
    dist = qDistTo(*ret);
    }

// NEW
  // NOTE: in original-game oCNpc::PerceptionCheck @0x0075dd30 rebuilds the candidate
  // list each cycle and re-classifies the PERC_ASSESSENEMY candidate via the live
  // GetPermAttitude (@0x0072fb30); there is no retained enemy that bypasses the
  // hostility test. Mirror the fresh-scan filter below (which already gates on
  // isEnemy) so a cached enemy whose attitude flipped to non-hostile is dropped.
  if(nearestEnemy!=nullptr &&
     (isEnemy(*nearestEnemy) && !nearestEnemy->isDown() &&
      canSenseNpc(*nearestEnemy,true)!=SensesBit::SENSE_NONE)) {
    ret  = nearestEnemy;
    dist = qDistTo(*ret);
    }
```

The change is a no-op in the common case (cached enemy still hostile) and only
alters behavior when the cached enemy is no longer hostile, where dropping it
matches the engine. `isEnemy(const Npc&)` exists at npc.cpp:4526 / npc.h:282.
