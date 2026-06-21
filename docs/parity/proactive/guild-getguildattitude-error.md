# Wld_GetGuildAttitude returns HOSTILE on bad guild; original returns NEUTRAL

**Confidence:** Medium

## Original function + address
`Wld_GetGuildAttitude` resolves through `oCNpc::GetGuildAttitude`
(**Gothic2.exe 0x007307d0**) into `oCGuilds::GetAttitude(int,int)`
(**Gothic2.exe 0x00700d40**). In `GetAttitude` the bounds test is
`if (g1 < 0x42 && g2 < 0x42)` (the guild table is a fixed 66-wide grid). On the
out-of-range branch it logs an "Unknown Guild" warning and returns the literal
value **2 = ATT_NEUTRAL** (the function's result register is set to 2 just before
return). The same NEUTRAL fallback is used for a null other-npc.

## OG location
`game/game/gamescript.cpp:1700-1704` — `GameScript::wld_getguildattitude` returns
`ATT_HOSTILE` (= 0) for any out-of-range guild index:
```
  if(gil1<0 || gil2<0 || gil1>=int(gilCount) || gil2>=int(gilCount))
    return ATT_HOSTILE; // error
```

## Divergence
A script querying `Wld_GetGuildAttitude` with an invalid/oversized guild id gets
**HOSTILE (0)** in OpenGothic versus **NEUTRAL (2)** in the original. Inverted
polarity of the error value: code that branches on the result (e.g. "if attitude
is hostile, attack") behaves oppositely on the error path. The in-range path is
correct; only the error sentinel differs.

## Proposed patch
`game/game/gamescript.cpp:1702`:
```
// OLD
  if(gil1<0 || gil2<0 || gil1>=int(gilCount) || gil2>=int(gilCount))
    return ATT_HOSTILE; // error

// NEW
  if(gil1<0 || gil2<0 || gil1>=int(gilCount) || gil2>=int(gilCount))
    // NOTE: in original-game oCGuilds::GetAttitude (Gothic2.exe 0x00700d40) the
    // out-of-range branch returns ATT_NEUTRAL (2), not ATT_HOSTILE.
    return ATT_NEUTRAL; // error
```
