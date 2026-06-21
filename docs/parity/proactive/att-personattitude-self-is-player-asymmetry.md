# personAttitude consults the NPC's perm/temp even when the *subject* is the player

**Confidence:** Medium-High

## Original function + address

`oCNpc::GetAttitude` (Gothic2.exe `0x0072fab0`), reached from the Daedalus
external `Npc_GetAttitude(self, other)` as `self->GetAttitude(other)`.

The original is **asymmetric** with respect to who the player is. Spelled out in prose,
`self->GetAttitude(other)` does:

- If `other` is **not** the player, OR (`self.tmpAttitude == self.permAttitude`):
  - If `other` **is** the player: return `self.permAttitude`.
  - Else (`other` is not the player): return `oCGuilds::GetAttitude(self.guild, other.guild)`
    — i.e. the pure guild-matrix value of *self's* guild toward *other's* guild. The NPC's
    personal perm/temp attitude is **never** consulted in this branch.
- Otherwise (`other` is the player and `tmp != perm`): return `self.tmpAttitude`.

The decisive case: when the **subject `self` is the player** and `other` is a regular NPC,
`other.IsAPlayer()` is false, so the engine takes the `guildAttitude(self.guild, other.guild)`
branch and returns the guild-matrix value. It does **not** look at the NPC's own
perm/temp attitude toward the player. The perm/temp attitudes only matter when the
*other* argument is the player (i.e. the subject is the NPC).

This is corroborated by the sibling getters at the same source location, which use the same
"`other` is the player?" gate: `oCNpc::IsHostile` (`0x0072f870`), `oCNpc::IsFriendly`
(`0x0072f900`), `oCNpc::GetPermAttitude` (`0x0072fb30`).

## OpenGothic file:line

`game/game/gamescript.cpp:1380` — `GameScript::personAttitude(const Npc &p0, const Npc &p1)`.

Specifically the selector at line 1385:

```cpp
const Npc& npc = p0.isPlayer() ? p1 : p0;
```

## Divergence

OpenGothic's `personAttitude` is **symmetric**: it picks `npc` as "whichever of the two is
not the player" and then consults *that* NPC's temp/perm/guild attitude. This is correct when
`p0` is the NPC and `p1` is the player (the common in-engine path: `isEnemy`, the
turn-toward-other check at `npc.cpp:569`, and most `Npc_GetAttitude(self_npc, hero)` script
calls — all pass the querying NPC as `p0`).

But when the **subject `p0` is the player** and `p1` is an NPC (e.g. a script doing
`Npc_GetAttitude(hero, someNpc)`), OpenGothic returns *the NPC's* personal perm/temp attitude,
whereas the original returns the pure guild-matrix value `guildAttitude(player.guild, npc.guild)`
and ignores that NPC's perm/temp. Concrete observable difference: a guard temp-angered at the
hero (`Npc_SetTempAttitude(guard, ATT_HOSTILE)`) queried as `Npc_GetAttitude(hero, guard)`
returns `ATT_HOSTILE` in OpenGothic but the guild value (typically `ATT_NEUTRAL`) in the
original.

Note this is **distinct** from the already-fixed "personAttitude temp-precedence" item, which
corrected the NPC-is-subject path (`p1` is the player). The bug here is purely the
subject-is-player ordering.

## Proposed patch

Gate the perm/temp consultation on `p1` (the `other` argument) being the player, matching the
original's `other.IsAPlayer()` test, instead of picking "whichever is not the player". When
`p0` is the player, fall through to the guild value.

OLD (`game/game/gamescript.cpp:1380`):
```cpp
Attitude GameScript::personAttitude(const Npc &p0, const Npc &p1) const {
  if(!p0.isPlayer() && !p1.isPlayer())
    return guildAttitude(p0,p1);

  Attitude att=ATT_NULL;
  const Npc& npc = p0.isPlayer() ? p1 : p0;
  // NOTE: in original-game oCNpc::GetAttitude (Gothic2.exe 0x0072fab0) the temp attitude
  ...
  att = npc.tempAttitude();
  if(att!=ATT_NULL && att!=npc.attitude())
    return att;
  att = npc.attitude();
  if(att!=ATT_NULL)
    return att;
  att = guildAttitude(p0,p1);
  return att;
  }
```

NEW:
```cpp
Attitude GameScript::personAttitude(const Npc &p0, const Npc &p1) const {
  // NOTE: in original-game oCNpc::GetAttitude (Gothic2.exe 0x0072fab0) is self->GetAttitude(other)
  // and only consults the SUBJECT's perm/temp attitude when the OTHER argument is the player.
  // When the subject itself is the player, the original returns the pure guild-matrix value
  // guildAttitude(player.guild, other.guild) and never looks at the NPC's perm/temp. Keying on
  // "whichever is not the player" wrongly surfaced a temp-angered NPC's attitude for
  // Npc_GetAttitude(hero, npc).
  if(!p1.isPlayer())
    return guildAttitude(p0,p1);

  // here p1 is the player, so the subject is p0 (the NPC); consult its temp/perm.
  Attitude att = p0.tempAttitude();
  if(att!=ATT_NULL && att!=p0.attitude())
    return att;
  att = p0.attitude();
  if(att!=ATT_NULL)
    return att;
  return guildAttitude(p0,p1);
  }
```

Grep-verified OG symbols used: `Npc::tempAttitude()` / `Npc::attitude()` (`npc.h:235,240`),
`GameScript::guildAttitude` (`gamescript.cpp:1373`), `Attitude`/`ATT_NULL` (`constants.h:244`),
`Npc::isPlayer()` (used throughout). `tempAttitude()` is `tmpAttitude`, `attitude()` is
`permAttitude`.

Caveat / why not higher confidence: the in-engine callers (`isEnemy`, the turn check, and the
typical `Npc_GetAttitude(self_npc, hero)` script usage) always pass the NPC as `p0`, so they are
unaffected and already correct. The change only alters the rarer `Npc_GetAttitude(hero, npc)`
(subject = player) result. The behavior is verifiable against the original but the surface is
narrow, hence Medium-High rather than High.
