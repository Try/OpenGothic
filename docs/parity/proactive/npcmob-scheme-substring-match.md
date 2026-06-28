# AI_UseMob mob lookup must substring-match the scheme name (not exact-equality)

**Confidence:** High

## Original fn + address
`oCNpc::EV_UseMob` (Gothic2.exe @0x00754290) handles the engine side of `AI_UseMob`.
On the first pass it upper-cases the requested mob name (`zSTRING::Upper`) and calls
`oCNpc::FindMobInter` (@0x0073fe70). FindMobInter collects mob candidates from a ±500
bounding box and, for each `oCMobInter`, matches by `zSTRING::Search(scheme, requestedName, ..., 1)`
(zSTRING::Search @0x0046c920). `zSTRING::Search` returns the index of the *first occurrence of the
pattern within the string* — here the pattern is the AI_UseMob scheme argument and the string is the
mob's own scheme name. A return value `>= 0` (substring found) plus a free-line-of-sight check makes
the mob a candidate; the nearest candidate wins. In other words the engine resolves a mob when the
**requested name is a (case-folded) substring of the mob's actual scheme name** — not by exact
equality. `Wld_IsMobAvailable` (@0x006f5e20) and `Wld_GetMobState` route through the same
FindMobInter substring match, and `Wld_IsMobValid`/spot lookups (`FindSpot` @0x007400e0) use the same
upper-case-substring convention.

## OG file:line
`/Users/admin/Downloads/opengothic/game/world/objects/interactive.cpp:463` — `Interactive::checkMobName`,
the sole predicate used by `WorldObjects::availableMob`
(`/Users/admin/Downloads/opengothic/game/world/worldobjects.cpp:874,883`), which backs `AI_UseMob`
(`npc.cpp:2857`), `Wld_IsMobAvailable`, and `Wld_GetMobState`.

## Divergence
`checkMobName` does exact equality `scheme==dest`:

```cpp
bool Interactive::checkMobName(std::string_view dest) const {
  std::string_view scheme = schemeName();
  if(scheme==dest)
    return true;
  return false;
  }
```

This is stricter than the original substring match. The OpenGothic `AI_UseMob` handler already
documents the consequence in a comment (`npc.cpp:2858-2864`): the L`Hiver 1.3 routine typo
`"COOL"` instead of `"BSCOOL"` — `"BSCOOL"` *contains* `"COOL"`, so the original engine still
resolved the cooking mob, but exact equality silently discards the command and the routine
interaction never happens. Any partial/abbreviated scheme argument used by a routine fails the same
way. (The handler also currently relies on the caller having pre-upper-cased `dest`; `ai_usemob`
at `gamescript.cpp:3484` does *not* upper-case, unlike `Wld_IsMobAvailable` at `gamescript.cpp:1829`,
so folding case here also closes that asymmetry — matching EV_UseMob's `zSTRING::Upper`.)

## Proposed patch (interactive.cpp:463)

OLD:
```cpp
bool Interactive::checkMobName(std::string_view dest) const {
  std::string_view scheme = schemeName();
  if(scheme==dest)
    return true;
  return false;
  }
```

NEW:
```cpp
bool Interactive::checkMobName(std::string_view dest) const {
  // NOTE: in original-game oCNpc::FindMobInter (Gothic2.exe @0x0073fe70) matches a mob with
  // zSTRING::Search(scheme, requestedName) (@0x0046c920): the AI_UseMob/Wld_IsMobAvailable scheme
  // argument is matched as a SUBSTRING of the mob's own scheme name, after EV_UseMob (@0x00754290)
  // upper-cases the request. Exact equality made the documented L`Hiver typo ("COOL" vs "BSCOOL")
  // -- and any partial scheme name -- fail to resolve the routine mob.
  std::string_view scheme = schemeName();
  if(dest.empty() || scheme.size()<dest.size())
    return false;
  auto up = [](char c){ return char(std::toupper((unsigned char)c)); };
  for(size_t i=0, e=scheme.size()-dest.size(); i<=e; ++i) {
    bool ok = true;
    for(size_t j=0; j<dest.size(); ++j)
      if(up(scheme[i+j])!=up(dest[j])) { ok=false; break; }
    if(ok)
      return true;
    }
  return false;
  }
```

(`<cctype>` for `std::toupper`; add the include if not already present in interactive.cpp.)
