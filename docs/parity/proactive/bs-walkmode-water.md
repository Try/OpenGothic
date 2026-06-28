# Walk/Sneak-mode toggle is not blocked in water (swim or dive)

**Confidence:** High

## Original fn + address

`oCNpc::EV_SetWalkMode` @0x006859f0 is the message handler for every walk/run/sneak
mode change (`oCMsgMovement` walk-mode subtype). Right after it resolves the NPC's
`oCAniCtrl_Human`, its very first guard calls `oCAniCtrl_Human::GetWaterLevel`
@0x006b89d0 (call site @0x00685a0d) and, if the result is `> 0`, returns `1`
immediately with no state change. `GetWaterLevel` returns `1` while swimming (surface)
and `2` while diving (fully submerged), so the original blocks ALL walk/run/sneak-mode
changes for ANY water level greater than zero. Because both the player's Walk/Sneak key
messages and the scripted `AI_SetWalkmode` command are dispatched through
`EV_SetWalkMode`, the original silently ignores walk/sneak toggles whenever the NPC is
in water. (This is the same family as the already-fixed `oCNpc::EV_TakeVob` @0x007534e0
water gate.)

## OG file:line

`game/world/objects/npc.cpp:551` — `Npc::setWalkMode(WalkBit m)` just assigns
`wlkMode = m` with no water gating. Its callers are the single dispatch surface that
the original routes through `EV_SetWalkMode`:
- `game/game/playercontrol.cpp:422` — `toggleWalkMode()` (player Walk key)
- `game/game/playercontrol.cpp:431` — `toggleSneakMode()` (player Sneak key; only gated
  by `canSneak()`, i.e. the SNEAK talent — no water check)
- `game/world/objects/npc.cpp:3038` — `AI_SetWalkMode` action handler

## Divergence

The original aborts walk/sneak-mode changes while `GetWaterLevel() > 0` (swim OR dive);
OG applies them unconditionally. Concretely, the player can toggle Sneak (and Walk)
while swimming/diving in OpenGothic, which the original silently rejects. OG's MoveAlgo
states are mutually exclusive, so `isSwim() || isDive()` is the exact equivalent of
`GetWaterLevel() > 0` (surface=swim, fully submerged=dive).

## Proposed patch

OLD (`game/world/objects/npc.cpp:551`):
```cpp
void Npc::setWalkMode(WalkBit m) {
  wlkMode = m;
  }
```

NEW:
```cpp
void Npc::setWalkMode(WalkBit m) {
  // NOTE: in original-game oCNpc::EV_SetWalkMode @0x006859f0 the walk/run/sneak-mode
  // message handler aborts immediately (returns 1, no state change) when
  // oCAniCtrl_Human::GetWaterLevel @0x006b89d0 > 0 -- i.e. while swimming (1) OR
  // diving (2). Both the player's Walk/Sneak toggles and the scripted AI_SetWalkmode
  // command route through this handler, so OG applying them unconditionally let the
  // player toggle sneak/walk while in water. MoveAlgo states are mutually exclusive,
  // so isSwim()||isDive() == GetWaterLevel()>0.
  if(isSwim() || isDive())
    return;
  wlkMode = m;
  }
```
