# Mobsi use-distance constant: 165 vs original 150

**Confidence: High**

## Original function

`oCMobInter::CanInteractWith` (Gothic2.exe `0x720f40`) and `oCMobInter::GetFreePosition`
(`0x71df50`) both call the virtual `oCMobInter::SearchFreePosition(npc, dist)` (`0x71dfc0`)
passing the float constant `0x43160000`, which is exactly **150.0**.

Inside `SearchFreePosition`, for each free ZS_POS slot the engine computes the squared
distance between the NPC's world position (vob offsets X/Y/Z) and the slot, and rejects
the slot when `dist*dist < squaredDistance` — i.e. the slot is accepted only if the NPC
is within **150** units of it. A slot that is reachable but farther than 150 triggers the
"too far away" manipulate message and interaction is refused.

So in vanilla G2 the maximum NPC->slot use distance for a mobsi is **150**.

## OpenGothic

`game/world/objects/interactive.cpp:796` (`Interactive::attach`):

```cpp
if((npc.centerPosition()-mv).quadLength()>MAX_AI_USE_DISTANCE*MAX_AI_USE_DISTANCE) {
```

with `MAX_AI_USE_DISTANCE = 165` (`game/game/constants.h:129`).

## Divergence

OpenGothic accepts the use-slot when the NPC is within **165** units, the original within
**150**. The player (and NPCs) can therefore start using chests, doors, benches, ladders,
etc. from ~10% farther than vanilla, and the "too far" rejection fires later than in the
original. `MAX_AI_USE_DISTANCE` is shared with several AI-reach paths (npc.cpp, gamescript.cpp),
so the fix is localized to the mobsi attach check rather than retuning the global constant.

## Proposed patch

File: `game/world/objects/interactive.cpp`

OLD:
```cpp
  if((npc.centerPosition()-mv).quadLength()>MAX_AI_USE_DISTANCE*MAX_AI_USE_DISTANCE) {
```

NEW:
```cpp
  // NOTE: in original-game oCMobInter::SearchFreePosition (Gothic2.exe 0x71dfc0) the
  // free-slot search distance passed from CanInteractWith/GetFreePosition is 150.0, not
  // MAX_AI_USE_DISTANCE (165); a slot is rejected once npc->slot exceeds 150.
  static const float MOBSI_USE_DISTANCE = 150.f;
  if((npc.centerPosition()-mv).quadLength()>MOBSI_USE_DISTANCE*MOBSI_USE_DISTANCE) {
```
