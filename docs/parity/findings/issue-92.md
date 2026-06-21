# Issue #92 — Chest labeled CHEST_LOBART

- Category: UI / focus-name display
- Disposition: **FIX** (surgical, low-risk)

## Problem
The chest with farmer's clothes in Lobart's house shows the raw VOB name
`CHEST_LOBART` as its inventory header, instead of the generic "Chest" the
original game shows. (Maintainer confirmed in-issue: original just shows "Chest".)

## OG files
- `game/world/objects/interactive.cpp` — `Interactive::displayName()` (lines 460-476)
- `game/world/objects/interactive.cpp` — `focName = vob.name;` (line 31)
- header consumer: `game/ui/inventorymenu.cpp:572` `drawHeader(p,chest->displayName(),...)`

## Original behavior (clean-room)
`oCMOB::GetName` (Gothic2.exe, addr 0x0071bc30, src oMobInter.cpp) resolves the
MOB name **only** through a Daedalus parser symbol index and returns that
symbol's *string value* via `zCParser::GetSymbol(idx)->GetValue()`. If the symbol
is absent or is not a string-valued symbol, the returned zSTRING stays **empty**,
and the focus UI falls back to the generic focus-mode label ("Chest"). The
original never echoes the raw VOB name.

## OG current behavior / divergence
`Interactive::displayName()` first does a **bare-name** symbol lookup:

```
string_frm strId(focName);                          // "CHEST_LOBART"
if(world.script().findSymbolIndex(strId)==size_t(-1))
  strId = string_frm("MOBNAME_",strId);             // only tried if bare fails
...
return s->get_string();
```

For this VOB, `focName == "CHEST_LOBART"`. A Daedalus symbol named
`CHEST_LOBART` exists (instance/func from the scripts), so the bare-name lookup
succeeds, and `get_string()` on a non-string symbol yields the symbol's own name
text — `CHEST_LOBART` — which is then shown. Normal chests have `focName=="CHEST"`
which only resolves via `MOBNAME_CHEST`, so they are unaffected.

Divergence: OG accepts a symbol of any type for the bare-name lookup, whereas the
original only ever yields a value when the resolved symbol is a string constant.

## Proposed patch
Require the resolved symbol to be a STRING-typed symbol (matching `oCMOB::GetName`,
which reads a string value and otherwise returns empty).

File: `game/world/objects/interactive.cpp`

OLD:
```cpp
std::string_view Interactive::displayName() const {
  if(focName.empty())
    return "";

  string_frm strId(focName);
  if(world.script().findSymbolIndex(strId)==size_t(-1))
    strId = string_frm("MOBNAME_",strId);

  if(world.script().findSymbolIndex(strId)==size_t(-1))
    return "";

  auto* s=world.script().findSymbol(strId);
  if(s==nullptr)
    return "";

  return s->get_string();
  }
```

NEW:
```cpp
std::string_view Interactive::displayName() const {
  if(focName.empty())
    return "";

  // NOTE: original oCMOB::GetName (Gothic2.exe 0x0071bc30, oMobInter.cpp) resolves
  // the name only as a STRING-valued Daedalus symbol; a non-string symbol that
  // merely collides with the VOB name (e.g. "CHEST_LOBART") yields an empty name,
  // so the focus UI falls back to the generic "Chest" label.
  string_frm strId(focName);
  if(world.script().findSymbolIndex(strId)==size_t(-1))
    strId = string_frm("MOBNAME_",strId);

  if(world.script().findSymbolIndex(strId)==size_t(-1))
    return "";

  auto* s=world.script().findSymbol(strId);
  if(s==nullptr || s->type()!=zenkit::DaedalusDataType::STRING)
    return "";

  return s->get_string();
  }
```

(`zenkit::DaedalusDataType` is already used in `game/game/gamescript.cpp`; include
`<zenkit/DaedalusScript.hh>` if not already transitively available in this TU.)

## Risk
Low. Only rejects non-string symbol collisions. Real `MOBNAME_*` entries are
string constants and continue to resolve. Result on the Lobart chest: empty name
-> generic "Chest" label, matching the original.
