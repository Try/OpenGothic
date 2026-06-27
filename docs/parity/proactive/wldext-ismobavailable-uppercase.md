# Wld_IsMobAvailable does not upper-case its mob-name argument

**Confidence:** Medium-High (code divergence verified against the original handler and against
OpenGothic's own already-applied parity pattern; runtime impact gated on scripts passing
non-upper-case mob names, which vanilla G2 does not, but mods/edge calls do).

## Original function + address (prose only)

The original `Wld_IsMobAvailable` handler (`Gothic2.exe`, oGameExternal.cpp thunk at
**0x006f5e20**) does the following: it reads the single string parameter from the parser, then
**immediately passes it through `zSTRING::Upper()`** (normalising the script-supplied mob/scheme
name to upper case) before resolving `self` and calling `oCNpc::FindMobInter(self, name)`. The
external returns true when a matching mob-interaction object is found. Notably the sibling handler
`Wld_GetMobState` (oGameExternal.cpp thunk at **0x006ed880**) does **not** upper-case its scheme
argument, so the normalisation is specific to `Wld_IsMobAvailable`.

This is the same `zSTRING::Upper()` query-normalisation that OpenGothic already deliberately mirrors
for other externals whose original uppercases a parser string before a case-sensitive name lookup —
e.g. `AI_GotoNextFP` (NOTE at `game/game/gamescript.cpp:3367`, original @0x006ec270) and
`Hlp_StrCmp` (NOTE at `game/gothic.cpp:1042`, original @0x6eebe0).

## OpenGothic file:line

`game/game/gamescript.cpp:1804` — `GameScript::wld_ismobavailable`:

```
bool GameScript::wld_ismobavailable(std::shared_ptr<zenkit::INpc> self, std::string_view name) {
  auto npc = findNpc(self);
  if(npc==nullptr) {
    return false;
    }
  auto wp = world().availableMob(*npc, name);   // <-- raw, un-normalised name
  return wp != nullptr;
  }
```

`WorldObjects::availableMob` (`game/world/worldobjects.cpp:866`) matches the scheme through
`Interactive::checkMobName` (`game/world/objects/interactive.cpp:446`), which does a
**case-sensitive exact** compare: `schemeName()==dest`. `Interactive::schemeName()`
(`interactive.cpp:535`) returns the raw `mesh->scheme` tag and is never upper-cased (unlike the mob
`owner` field, which is upper-cased at `interactive.cpp:53`).

## Divergence

Original: `Wld_IsMobAvailable("bed")` → query upper-cased to `"BED"` → matches a mob whose scheme is
`BED` → returns true.

OpenGothic: `Wld_IsMobAvailable("bed")` → `checkMobName` compares `"BED"=="bed"` → false → returns
false, even though the mob exists and is available.

So a script (or mod) that passes a non-upper-case mob/scheme name gets `false` from OpenGothic where
the original returns `true`. Vanilla G2 scripts pass upper-case scheme tags, so the bug is latent
there, but OpenGothic has elected to reproduce this exact `Upper()` normalisation for every other
affected external; `Wld_IsMobAvailable` is the one that was missed.

## Proposed patch

Upper-case the name in the external before the lookup, mirroring the established
`ai_gotonextfp` pattern (grep-verified symbols: `std::toupper`, `world().availableMob`,
`std::string`).

OLD (`game/game/gamescript.cpp:1804`):
```
bool GameScript::wld_ismobavailable(std::shared_ptr<zenkit::INpc> self, std::string_view name) {
  auto npc = findNpc(self);
  if(npc==nullptr) {
    return false;
    }

  auto wp = world().availableMob(*npc, name);
  return wp != nullptr;
  }
```

NEW:
```
bool GameScript::wld_ismobavailable(std::shared_ptr<zenkit::INpc> self, std::string_view name) {
  auto npc = findNpc(self);
  if(npc==nullptr) {
    return false;
    }

  // NOTE: in original-game Wld_IsMobAvailable @0x006f5e20 the mob-name argument is upper-cased
  // (zSTRING::Upper) before FindMobInter; OpenGothic compares scheme names case-sensitively
  // (Interactive::checkMobName), so a non-upper-case name never matched. (Wld_GetMobState
  // @0x006ed880 does NOT upper-case, so this normalisation is intentionally local to this external.)
  std::string mob {name};
  for(auto& c:mob)
    c = char(std::toupper(c));

  auto wp = world().availableMob(*npc, mob);
  return wp != nullptr;
  }
```

(`WorldObjects::availableMob` / `World::availableMob` take `std::string_view`, so the `std::string`
binds without an extra overload.)
