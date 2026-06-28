# Npc_GetActiveSpellLevel returns 0 instead of -1 when no spell is active

**Confidence:** Medium

## Original fn + address
The `Npc_GetActiveSpellLevel` external handler (Gothic2.exe `FUN_006e5a40`) forwards to
`oCNpc::GetActiveSpellLevel` at `0x0073cfe0`. That method first normalizes the NPC fight/weapon
mode field (offset `+0x250`) into range, then returns a spell level **only** when the NPC is in
magic mode (mode `== 7`) and has a mag-book with a selected spell: it returns
`oCSpell::GetLevel(selectedSpell)`. In every other path — not in magic mode, no mag-book, or no
selected spell — the method returns **`-1`**. (Its siblings `oCNpc::GetActiveSpellCategory`
@`0x0073cfa0` and `oCNpc::GetActiveSpellNr` @`0x0073cf60` are structurally identical and also
return `-1` in the no-spell path; the Category one was already corrected in OpenGothic.)

## OG file:line
`/Users/admin/Downloads/opengothic/game/game/gamescript.cpp:3208` (`npc_getactivespelllevel`),
backed by `Npc::activeSpellLevel()` at
`/Users/admin/Downloads/opengothic/game/world/objects/npc.cpp:4414`.

## Divergence
```cpp
int GameScript::npc_getactivespelllevel(std::shared_ptr<zenkit::INpc> npcRef) {
  int  v   = 0;
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    v = npc->activeSpellLevel();
  return v;
  }
```
`Npc::activeSpellLevel()` returns a 1-based level only while a cast/invest is in progress
(`CS_Cast_0..CS_Cast_Last` / `CS_Invest_0..CS_Invest_Last`), and otherwise returns `0`
(`npc.cpp:4419`). So for a missing NPC, an NPC not in magic mode, or an NPC with magic drawn but no
cast in progress, OpenGothic returns **`0`** whereas the original returns **`-1`**. This is the
exact same sentinel-collision class that was already fixed for `Npc_GetActiveSpellCat`
(`gamescript.cpp:3183`, which now returns `-1`). Daedalus spell AI that treats `-1` as the
"no active spell" marker (the same convention `Npc_GetActiveSpellNr`/`...Cat` use) will misread the
OpenGothic `0` as a valid level.

## Proposed patch
```cpp
// OLD
int GameScript::npc_getactivespelllevel(std::shared_ptr<zenkit::INpc> npcRef) {
  int  v   = 0;
  auto npc = findNpc(npcRef);
  if(npc!=nullptr)
    v = npc->activeSpellLevel();
  return v;
  }

// NEW
int GameScript::npc_getactivespelllevel(std::shared_ptr<zenkit::INpc> npcRef) {
  // NOTE: in original-game oCNpc::GetActiveSpellLevel (Gothic2.exe 0x0073cfe0) returns -1 when the
  // NPC is not in magic mode / has no selected spell, matching its siblings GetActiveSpellNr
  // (0x0073cf60) and GetActiveSpellCategory (0x0073cfa0). OpenGothic returned 0, which collides with
  // a real (non-existent, since levels are 1-based) level and breaks script tests that use -1 as the
  // "no active spell" marker.
  auto npc = findNpc(npcRef);
  if(npc==nullptr)
    return -1;
  int v = npc->activeSpellLevel();
  if(v<=0)
    return -1;
  return v;
  }
```

Caveat (reason confidence is Medium, not High): OpenGothic derives the level from the live
cast-state (`Npc::activeSpellLevel`) rather than from the mag-book's selected-spell `GetLevel()`,
so in the narrow "magic drawn, spell selected, but no cast yet in flight" state the original may
report a positive selected-spell level where this patch returns `-1`. The patch nonetheless aligns
the dominant no-active-spell sentinel with the original method and with the already-accepted
`Npc_GetActiveSpellCat` fix.
