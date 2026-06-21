# Issue #639 — More perc issues

## Issue
Three perception complaints:
1. Milten talks through a wall in old-mage building (`Npc::canSenseNpc` adds `SENSE_HEAR` when noisy, ignoring rooms).
2. NPCs wake when the player picks a lock while sneaking — the `BS_SNEAK` bodystate check fails because lockpicking changes the bodystate to a MOBSI-interact state, so the persistent walk-mode sneak flag should be checked instead.
3. `AssessEnterRoom` not sent while sneaking (a `BS_SNEAK` early-return in `sendImmediatePerc`).

## OG files
- game/world/objects/npc.cpp (`canSenseNpc`, `tickAnimationTags`)
- game/world/worldobjects.cpp (`sendImmediatePerc`, `passivePerceptionProcess`)

## Original behavior (prose)
In the original Gothic 2 engine, sneaking is a persistent walk-mode flag (the `keySneak` toggle, `WM_Sneak`), not a transient animation bodystate. Quiet-sound percepts (`ASSESSQUIETSOUND`) emitted by footstep/ground-sound animation events are suppressed while the player is in sneak walk-mode, and that suppression persists across MOBSI interactions such as lock-picking, because the walk-mode flag is unchanged by entering an interaction.

## OG current file:line
- Point 1 already resolved: `canSenseNpc(const Npc&,...)` hardcodes `const bool isNoisy = false;` (npc.cpp:4697), so the room-ignoring `SENSE_HEAR` branch is dead for NPC-vs-NPC sensing.
- Point 3 already resolved: `WorldObjects::sendImmediatePerc` (worldobjects.cpp:917) no longer contains the `pl->bodyStateMasked()==BS_SNEAK` early-return quoted in the report.
- Point 2 NOT resolved: npc.cpp:2336

```
  if(ev.groundSounds>0 && isPlayer() && bodyStateMasked()!=BodyState::BS_SNEAK)
    world().sendImmediatePerc(*this,*this,*this,PERC_ASSESSQUIETSOUND);
```

## Divergence
While lock-picking (or any MOBSI), `bodyStateMasked()` returns the interaction bodystate, not `BS_SNEAK`, so the condition is true and a `PERC_ASSESSQUIETSOUND` is emitted, waking nearby guards even though the player is sneaking. Checking the persistent `WM_Sneak` walk-mode flag instead matches the original behavior and is what the reporter requested (the change rejected in PR #589 and never re-landed in commit fccda5b).

## Proposed patch
File: game/world/objects/npc.cpp

OLD:
```cpp
  if(ev.groundSounds>0 && isPlayer() && bodyStateMasked()!=BodyState::BS_SNEAK)
    world().sendImmediatePerc(*this,*this,*this,PERC_ASSESSQUIETSOUND);
```
NEW:
```cpp
  // NOTE: in original-game sneaking is a persistent walk-mode flag (WM_Sneak),
  // not a transient bodystate, so quiet-sound percs stay suppressed during MOBSI
  // interactions (e.g. lock-picking) while the player is sneaking. Issue #639.
  if(ev.groundSounds>0 && isPlayer() && (wlkMode&WalkBit::WM_Sneak)!=WalkBit::WM_Sneak)
    world().sendImmediatePerc(*this,*this,*this,PERC_ASSESSQUIETSOUND);
```

Notes:
- `wlkMode` is a member of `Npc` (npc.cpp:562) and `WalkBit::WM_Sneak` plus `operator&` are defined in game/game/constants.h. Scope is limited to player ground-sound events, so risk is minimal.
- Points 1 and 3 require no change in the current tree.
