# FP name not upper-cased in AI_GotoFP / Wld_IsFPAvailable / Wld_IsNextFPAvailable

**Confidence:** High

## Original fn + address

In Gothic2.exe every Daedalus external that takes a free-point name argument first
normalizes that argument with `zSTRING::Upper` before it is ever used for spot-name
matching:

- `AI_GotoFP` external (`oGameExternal.cpp`, `FUN_006ebfa0`): `GetParameter` -> `zSTRING::Upper(fpName)`
  -> builds the `oCMsgMovement` whose name field is later consumed by `oCNpc::EV_GotoFP` @0x00685700,
  which calls `oCNpc::FindSpot` @0x007400e0 with that (already upper-cased) name.
- `Wld_IsFPAvailable` external (`FUN_006eb5b0`): `GetParameter` -> `zSTRING::Upper(fpName)`
  -> `oCNpc::FindSpot(npc, fpName, 1, 700.0)`.
- `Wld_IsNextFPAvailable` external (`FUN_006eb860`): `GetParameter` -> `zSTRING::Upper(fpName)`
  -> `oCNpc::FindSpot(npc, fpName, 0, 700.0)`.

The per-candidate name test inside `oCNpc::FindSpot` @0x007400e0 is `zSTRING::Search(spotName, fpName)`
@0x0046c920 (a case-sensitive sub-string search), and the loaded `zCVobSpot` names are upper-case.
Hence the engine's `Upper` of the script argument is exactly what makes a lower/mixed-case script
name still match an upper-case spot name. The sibling external `AI_GotoNextFP` (`FUN_006ec270`)
performs the same `zSTRING::Upper`, and OpenGothic *already* mirrors it (see
`docs/parity/proactive/ext5-ai-gotonextfp-uppercase.md` and `GameScript::ai_gotonextfp`).

## OG file:line

- `game/game/gamescript.cpp:3393` — `GameScript::ai_gotofp` passes `waypoint` straight to
  `World::findFreePoint`, no upper-casing.
- `game/game/gamescript.cpp:1790` — `GameScript::wld_isfpavailable` passes `name` straight to
  `World::findFreePoint`, no upper-casing.
- `game/game/gamescript.cpp:1799` — `GameScript::wld_isnextfpavailable` passes `name` straight to
  `World::findNextFreePoint`, no upper-casing.

OpenGothic stores all way/free-point names upper-cased (`game/world/waypoint.cpp:9` `upcaseof`,
applied in every `WayPoint` ctor), and `WayPoint::checkName` (`game/world/waypoint.cpp:42`) matches
case-sensitively (`name==n` / `name.find(n)`). So a non-upper-case script fp-name argument never
matches in OpenGothic, whereas the original matches it after `Upper`.

## Divergence

These three FP externals are the exact analogue of the already-fixed `ai_gotonextfp`: the original
upper-cases the script-supplied free-point name; OpenGothic does not. A script call such as
`AI_GotoFP(self,"fp_roam")` or `Wld_IsFPAvailable(self,"Fp_Stand_Guarding")` resolves a spot in the
original but silently returns "no spot" / FALSE in OpenGothic (NPC fails to walk to the FP, or the
availability query is wrong). Gothic scripts overwhelmingly use upper-case FP literals, so impact is
limited to mods / scripts that use mixed/lower case — the same justification under which the
`ai_gotonextfp` fix was already accepted.

## Proposed patch

Mirror the existing `ai_gotonextfp` fix (which carries the `@0x006ec270` NOTE). Upper-case the
fp-name at each external boundary before passing it to the FP lookup.

`game/game/gamescript.cpp` — `ai_gotofp`:

OLD:
```cpp
void GameScript::ai_gotofp(std::shared_ptr<zenkit::INpc> npcRef, std::string_view waypoint) {
  auto npc = findNpc(npcRef);

  if(npc) {
    auto to = world().findFreePoint(*npc,waypoint);
    if(to!=nullptr)
      npc->aiPush(AiQueue::aiGoToPoint(*to));
    }
  }
```
NEW:
```cpp
void GameScript::ai_gotofp(std::shared_ptr<zenkit::INpc> npcRef, std::string_view waypoint) {
  auto npc = findNpc(npcRef);

  if(npc) {
    // NOTE: in original-game AI_GotoFP external (Gothic2.exe oGameExternal.cpp @0x006ebfa0) the
    // free-point name is upper-cased (zSTRING::Upper) before EV_GotoFP/FindSpot @0x007400e0 match
    // it against the (upper-case) spot names. OpenGothic stores FP names upper-cased and
    // WayPoint::checkName matches case-sensitively, so a non-upper-case script name never matched.
    std::string name {waypoint};
    for(auto& c:name)
      c = char(std::toupper(c));
    auto to = world().findFreePoint(*npc,name);
    if(to!=nullptr)
      npc->aiPush(AiQueue::aiGoToPoint(*to));
    }
  }
```

`game/game/gamescript.cpp` — `wld_isfpavailable`:

OLD:
```cpp
  auto wp = world().findFreePoint(*findNpc(self.get()),name);
  return wp!=nullptr;
```
NEW:
```cpp
  // NOTE: in original-game Wld_IsFPAvailable external (Gothic2.exe @0x006eb5b0) the fp-name is
  // upper-cased (zSTRING::Upper) before FindSpot @0x007400e0 substring-matches the upper-case spot
  // names; OpenGothic stores FP names upper-cased and matches case-sensitively.
  std::string fp {name};
  for(auto& c:fp)
    c = char(std::toupper(c));
  auto wp = world().findFreePoint(*findNpc(self.get()),fp);
  return wp!=nullptr;
```

`game/game/gamescript.cpp` — `wld_isnextfpavailable`: same upper-casing before
`world().findNextFreePoint(...)`, NOTE citing `Wld_IsNextFPAvailable @0x006eb860`.

(A single shared helper, or upper-casing inside `World::findFreePoint`/`findNextFreePoint`, is also
viable; keeping it at the external boundary matches where the original applies `Upper` and avoids
touching the non-FP waypoint-name callers of `checkName`.)
