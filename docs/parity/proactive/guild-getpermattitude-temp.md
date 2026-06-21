# Npc_GetPermAttitude wrongly applies temp-attitude precedence

**Confidence:** High

## Original function + address
`Npc_GetPermAttitude` (script external) maps to `oCNpc::GetPermAttitude(oCNpc*)`
at **Gothic2.exe 0x0072fb30**. Its decompilation is unambiguous and short:
- If the *other* npc is the player, it returns the npc's **permanent** attitude
  field (oCNpc+0x7e4) directly.
- Otherwise it returns `oCGuilds::GetAttitude(selfGuild, otherGuild)`.

It **never** reads the temp-attitude field (oCNpc+0x7e8). Compare with the sibling
`oCNpc::GetAttitude` at 0x0072fab0, which *does* let temp override perm toward the
player when `tmpAtt != permAtt`. So in the original the two externals differ:
`Npc_GetAttitude` honors temp, `Npc_GetPermAttitude` ignores it (perm-only).

## OG location
`game/game/gamescript.cpp:2700` — `GameScript::npc_getpermattitude` calls
`personAttitude(*a,*b)`, exactly the same helper used by `npc_getattitude`
(line 2689). `personAttitude` (line 1380) applies temp-attitude precedence
(`tmpAttitude` overrides when `!=ATT_NULL && != permAttitude`).

The in-file comment at line 1390 even states the intended behavior
("Npc_GetPermAttitude stays perm-only"), but the code routes through the
temp-aware path, so the intent was never implemented.

## Divergence
For an NPC whose temp attitude has been changed (e.g. a guard temporarily angered
by a crime via `Npc_SetTempAttitude`), a script calling
`Npc_GetPermAttitude(guard, hero)` returns the **temporary** attitude in OpenGothic
but the **permanent** attitude in the original game. Daedalus logic that queries the
permanent attitude to decide long-term behavior (faction checks, dialog gating) will
read the wrong value while a temp attitude is active. Gameplay-different and
unambiguous.

## Proposed patch
Add a perm-only helper (mirrors `personAttitude` minus the temp step) and use it
from `npc_getpermattitude`.

`game/game/gamescript.cpp` — add after `personAttitude` (after line 1399):
```
// OLD  (nothing; new function inserted after personAttitude)

// NEW
Attitude GameScript::permAttitude(const Npc &p0, const Npc &p1) const {
  // NOTE: in original-game oCNpc::GetPermAttitude (Gothic2.exe 0x0072fb30) the
  // permanent attitude is returned for the player target and the guild attitude
  // otherwise; the temp attitude (oCNpc+0x7e8) is never consulted here, unlike
  // oCNpc::GetAttitude (0x0072fab0). Keep this perm-only.
  if(!p0.isPlayer() && !p1.isPlayer())
    return guildAttitude(p0,p1);
  const Npc& npc = p0.isPlayer() ? p1 : p0;
  Attitude att = npc.attitude();
  if(att!=ATT_NULL)
    return att;
  return guildAttitude(p0,p1);
  }
```

`game/game/gamescript.h:166` — declare it next to `personAttitude`:
```
// OLD
    Attitude personAttitude(const Npc& p0,const Npc& p1) const;

// NEW
    Attitude personAttitude(const Npc& p0,const Npc& p1) const;
    Attitude permAttitude(const Npc& p0,const Npc& p1) const;
```

`game/game/gamescript.cpp:2700`:
```
// OLD
    auto att=personAttitude(*a,*b);
    return att;

// NEW
    // NOTE: in original-game Npc_GetPermAttitude (oCNpc::GetPermAttitude,
    // Gothic2.exe 0x0072fb30) is perm-only and ignores temp attitude.
    auto att=permAttitude(*a,*b);
    return att;
```
