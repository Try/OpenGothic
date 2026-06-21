# Attitude: temp attitude ignored in effective person-attitude

**Confidence:** High

## Original function + address

`oCNpc::GetAttitude(this, other)` @ `0x0072fab0` (`Npc_GetAttitude` script external).

In prose, the original computes the *effective* attitude of `this` (an NPC) toward
`other` as follows:

- If `other` is NULL, return ATT_NEUTRAL.
- If `other` is the player **and** the NPC's temp-attitude field differs from its
  perm-attitude field, return the **temp** attitude.
- Otherwise, if `other` is the player, return the **perm** attitude.
- Otherwise (`other` is not the player), return the guild-vs-guild attitude
  (`oCGuilds::GetAttitude(this.guild, other.guild)`).

i.e. the temporary attitude (set by `Npc_SetTempAttitude`, used by guards reacting
to crimes / temporary anger) takes precedence over the permanent attitude, but only
when evaluating attitude *toward the player*.

The sibling `oCNpc::GetPermAttitude` @ `0x0072fb30` (`Npc_GetPermAttitude`) is the one
that deliberately skips temp and returns perm/guild only.

## OpenGothic location

`game/game/gamescript.cpp:1379` — `GameScript::personAttitude`.

It picks the non-player NPC and reads only `npc.attitude()` (which returns
`permAttitude`, see `game/world/objects/npc.h:235`). `tmpAttitude` /
`tempAttitude()` (`npc.h:240`) is never consulted here, and the same function backs
`npc_getattitude` (`gamescript.cpp:2674`, with an explicit `//TODO: temp attitudes`),
`Npc::isEnemy` (`npc.cpp:4144`), fight-target selection (`gamescript.cpp:2477`) and
friendly-fire (`gamescript.cpp:1396`).

## Divergence

`Npc_GetAttitude` in the original returns the temp attitude when it has been set
differently from perm (vs. the player); OpenGothic returns perm/guild instead.
Gameplay effect: an NPC temporarily angered/pacified toward the player via
`Npc_SetTempAttitude` is not treated as such by enemy detection, fight logic, or the
`Npc_GetAttitude` script call. `Npc_GetPermAttitude` is correctly perm-only and must
stay unchanged.

## Proposed patch

`game/game/gamescript.cpp`

OLD:
```cpp
Attitude GameScript::personAttitude(const Npc &p0, const Npc &p1) const {
  if(!p0.isPlayer() && !p1.isPlayer())
    return guildAttitude(p0,p1);

  Attitude att=ATT_NULL;
  const Npc& npc = p0.isPlayer() ? p1 : p0;
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
  if(!p0.isPlayer() && !p1.isPlayer())
    return guildAttitude(p0,p1);

  Attitude att=ATT_NULL;
  const Npc& npc = p0.isPlayer() ? p1 : p0;
  // NOTE: in original-game oCNpc::GetAttitude (0x0072fab0) the temp attitude
  // (Npc_SetTempAttitude) overrides perm when set differently from perm, but only
  // for attitude toward the player. Npc_GetPermAttitude (0x0072fb30) stays perm-only.
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

(`npc_getpermattitude` at `gamescript.cpp:2680` should be left calling a perm-only
path, not `personAttitude`, but that is a separate cleanup — the bug above is the
effective-attitude one.)
