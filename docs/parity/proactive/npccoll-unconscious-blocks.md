# NPC-vs-NPC soft collision: unconscious bodies wrongly block other NPCs

**Confidence:** Medium

## Original function + address (prose only)
In the original game, going "down" routes through two sibling transitions that set the
same *lying* body-state on the NPC: the death path and `oCNpc::DropUnconscious`
(`Gothic2.exe @0x00735eb0`, already cited in `game/world/objects/npc.cpp:2168`).
`DropUnconscious` flips the body-state word (offset 0x76c) to `BS_UNCONSCIOUS`, starts the
T_STAND_2_WOUNDED lying animation, and — crucially — does **not** alter the vob's dynamic
collision flag (no `zCVob::SetCollDetDyn` call; verified in the decompiled body). The dead
transition behaves identically: a downed NPC is a lying "corpse" that the moving-NPC AI does
not treat as a movement blocker. Observable in-game: living NPCs walk over/through both dead
**and** unconscious (fist-fight KO) bodies — neither blocks crowd movement. The two down
states are symmetric for NPC-vs-NPC collision.

## OpenGothic file:line
`game/world/objects/npc.cpp:614-615` (inside `Npc::onNoHealth`), with the re-enable
counterpart at `game/world/objects/npc.cpp:580` (`Npc::checkHealth`).

OpenGothic models "corpse is walk-through" by disabling the soft NPC capsule
(`physic.setEnable(false)` → `DynamicWorld::NpcItem::setEnable`, dynamicworld.cpp:1117, which
drops the body from the npc-vs-npc list checked at dynamicworld.cpp:341 `v.body->enable`).

## Divergence
`onNoHealth(bool death, …)` is only ever reached when the NPC is going down — its three
callers pass `death=true` for the lethal/already-dead case (npc.cpp:571, npc.cpp:2172) and
`death=false` for the non-lethal KO that starts `ZS_Unconscious` (npc.cpp:576). But the soft
capsule is dropped on the **narrower** condition `death` only:

```
  if(death)
    physic.setEnable(false);
```

So a dead NPC's capsule is removed (walk-through, matches original) while an **unconscious**
NPC keeps a full upright soft capsule active. Because the capsule is a fixed vertical
ellipsoid anchored at the NPC origin regardless of the lying animation, a KO'd body becomes a
standing-height obstacle: other NPCs collide with / are deflected by it (movealgo.cpp
`onMoveFailed`, with `info.npc!=nullptr`) until the victim is revived. This is also an
internal inconsistency — OG already treats dead bodies as non-blocking but not the
otherwise-identical unconscious lying body. The re-enable side is already correct/symmetric:
`checkHealth` calls `physic.setEnable(true)` (npc.cpp:580) only once the NPC is alive **and**
no longer unconscious (npc.cpp:559-564), so dropping the capsule for both down states cannot
strand a revived NPC without collision.

## Proposed patch
`game/world/objects/npc.cpp`, `Npc::onNoHealth` (line 614-615):

OLD:
```
  if(death)
    physic.setEnable(false);
```
NEW:
```
  // NOTE: in original-game oCNpc::DropUnconscious @0x00735eb0 sets the same lying body-state
  // as the dead path and never re-flags dynamic collision; downed NPCs (dead OR unconscious)
  // are walk-through corpses for NPC-vs-NPC movement. onNoHealth is only entered when the NPC
  // goes down, so drop the soft npc capsule in both cases (checkHealth re-enables on revive).
  physic.setEnable(false);
```

Secondary (same root cause, lower impact): the save-load path at npc.cpp:326-327 only disables
the capsule for `isDead()`. For full symmetry it should use `isDown()` (declared npc.cpp:4355,
`isUnconscious() || isDead()`) so an NPC saved while unconscious also loads non-blocking. Left
out of the primary patch since being saved mid-KO is rare; flag as follow-up rather than
bundling.

Grep-verified symbols: `Npc::onNoHealth` (npc.cpp:584, decl npc.h:519); member `physic` is
`DynamicWorld::NpcItem` (npc.h:563); `NpcItem::setEnable(bool)` (dynamicworld.h:97,
dynamicworld.cpp:1117); `Npc::isDown()` (npc.cpp:4355); `Npc::isUnconscious()` (npc.cpp:4351).
