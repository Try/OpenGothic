# AssessEnterRoom perception suppressed by transient bodystate instead of persistent sneak walk-mode

## Confidence
High

## Original function + address (prose)
In the original Gothic 2 engine, the player's portal-room transition is detected by the per-frame
room tracker `oCPortalRoomManager::HasPlayerChangedPortalRoom` (Gothic2.exe @0x00773070), which
caches the current/former sector name returned by `zCVob::GetSectorNameVobIsIn` (@0x00600ae0,
backed by `zCBspTree::GetSectorNameVobIsIn` @0x00535180) and fires the `PERC_ASSESSENTERROOM`
assessment to NPCs in the room the player just entered. As established and documented in the
issue-639 parity analysis (docs/parity/findings/issue-639.md), the original models "is the player
sneaking" as a *persistent walk-mode flag* (the `keySneak` toggle, `WM_Sneak`) rather than a
transient animation bodystate; the engine suppresses the player-noise/stealth assessments
(`ASSESSQUIETSOUND` footsteps and the room-entry `ASSESSENTERROOM`) while that walk-mode flag is
set. The walk-mode flag is unchanged by entering a MOBSI interaction (e.g. lock-picking), so the
suppression persists across interactions; only the body-state changes.

## OpenGothic file:line
game/game/movealgo.cpp:165

```cpp
if(cache.sector!=nullptr && portal!=cache.sector) {
    formerPortal = portal;
    portal       = cache.sector;
    if(npc.isPlayer() && npc.bodyStateMasked()!=BS_SNEAK) {   // <-- transient bodystate gate
      auto& w = npc.world();
      w.sendImmediatePerc(npc,npc,npc,PERC_ASSESSENTERROOM);
      }
    }
```

## Divergence
This is the same defect class that issue-639 (point 2) already fixed for the *sibling* perception
`PERC_ASSESSQUIETSOUND` in `Npc::tickAnimationTags` (game/world/objects/npc.cpp:2426), but the
room-entry trigger in `MoveAlgo::tick` was left on the old, unfaithful mechanism. OpenGothic gates
`ASSESSENTERROOM` on the *transient* `bodyStateMasked()!=BS_SNEAK`, whereas the original gates it on
the *persistent* `WM_Sneak` walk-mode flag. Whenever the player crosses a portal-room boundary while
in sneak walk-mode but with a body-state that is momentarily not `BS_SNEAK` (any animation/transition
or MOBSI-interact state that leaves `BS_SNEAK` while `WM_Sneak` is still toggled), OpenGothic wrongly
emits the room-entry assessment and alerts guards even though the player is stealthing — exactly the
"NPCs wake while sneaking" symptom #639 describes, here for room entry instead of footsteps. The two
sibling perceptions are even handled together in `WorldObjects::passivePerceptionProcess`
(worldobjects.cpp:969), underscoring they should use the same sneak test.

## Proposed patch
File: game/game/movealgo.cpp

OLD:
```cpp
    if(npc.isPlayer() && npc.bodyStateMasked()!=BS_SNEAK) {
      auto& w = npc.world();
      w.sendImmediatePerc(npc,npc,npc,PERC_ASSESSENTERROOM);
      }
```
NEW:
```cpp
    // NOTE: in original-game sneaking is a persistent walk-mode flag (WM_Sneak), not a transient
    // bodystate, so the room-entry assessment stays suppressed while the player sneaks across a
    // portal boundary (and across MOBSI interactions). Mirrors the issue-639 ASSESSQUIETSOUND fix
    // in Npc::tickAnimationTags; player room-change is tracked by oCPortalRoomManager
    // ::HasPlayerChangedPortalRoom @0x00773070 in Gothic2.exe.
    if(npc.isPlayer() && (npc.walkMode()&WalkBit::WM_Sneak)!=WalkBit::WM_Sneak) {
      auto& w = npc.world();
      w.sendImmediatePerc(npc,npc,npc,PERC_ASSESSENTERROOM);
      }
```

Notes:
- Grep-verified symbols: `Npc::walkMode()` public accessor (game/world/objects/npc.h:118),
  `WalkBit::WM_Sneak` (game/game/constants.h:222), and `operator&` on `WalkBit` already used in
  this same file (game/game/movealgo.cpp:190: `npc.walkMode() & WalkBit::WM_Walk`). `WalkBit` is
  thus already in scope in movealgo.cpp.
- Surgical and build-verifiable: one condition swapped, scope limited to the player's room-entry
  assessment; identical in form to the already-landed #639 ASSESSQUIETSOUND fix.
