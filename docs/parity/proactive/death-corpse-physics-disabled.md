# Death: dead-NPC collision body permanently disabled (corpses are walk-through / projectile-pass-through)

**Confidence:** Medium-High (root cause is certain; the original explicitly re-enables physics on death. Risk: OpenGothic may have disabled it deliberately to stop the upright capsule from blocking, so the fix needs in-game verification before landing — hence not a blind apply.)

## Original function + address (prose only)

`oCNpc::DoDie` at `0x00736760` is the death handler (analogue of OpenGothic's `onNoHealth(death=true)`).
After dropping in-hand items (`DropAllInHand`), stopping turn/burn anims, switching the AI state to the
dead state (StartAIState with the dead-state index), and firing the murder passive perception
(`CreatePassivePerception(this, 6 /* PERC_ASSESSMURDER */, killer, this)` only when a killer vob is
present), the function ends by calling `zCVob::SetPhysicsEnabled(this, 1)` — i.e. the corpse's collision
body is left **ENABLED**. The companion `oCNpc::DropUnconscious` at `0x00735eb0` follows the same shape
for the unconscious case. This contrasts the two collision-detection flag writes: `DoDie` writes the
NPC's movement-collision field (`+0x1b8`) to 0 while `DropUnconscious` writes it to 1, but the vob-level
physics (bounding-box collision used for ray/projectile tests and body-vs-body overlap) is explicitly
turned back on for the dead body.

## OpenGothic file:line

- `game/world/objects/npc.cpp:614-615` — in `Npc::onNoHealth`, `if(death) physic.setEnable(false);`
- `game/world/objects/npc.cpp:326-327` — in the load path, dead NPCs are loaded with `physic.setEnable(false)`.
- `game/physics/dynamicworld.cpp:1117-1122` — `DynamicWorld::NpcItem::setEnable` toggles `obj->enable`.
- `game/physics/dynamicworld.cpp:265-266` — `rayTest` early-returns `false` when `!npc.enable`.
- `game/physics/dynamicworld.cpp:341` — body-vs-body `hasCollision` is skipped when `!v.body->enable`.

## Divergence

OpenGothic permanently **disables** the NPC's collision body the instant an NPC dies, and re-applies the
disable when loading an already-dead NPC. Because `DynamicWorld` gates both ray queries
(`rayTest`, line 265) and body overlap (`hasCollision`, line 341) on `enable`, an OpenGothic corpse:

- cannot be hit by arrows/bolts/ray spells (projectiles pass straight through), and
- offers no body-vs-body collision (the player and other NPCs walk through the corpse freely).

The original game leaves the corpse's vob physics **enabled** (`SetPhysicsEnabled(this,1)` at the tail of
`DoDie`), so a fresh corpse keeps its bounding-box collision: it can be shot/struck and you bump into it
rather than passing through. Only the *movement* collision flag (`+0x1b8`) is cleared, not the vob physics.

This is the death-subsystem twin of the unconscious path: in OpenGothic `onNoHealth(death=false)` does NOT
disable physics (correct — matches `DropUnconscious`), but the `death=true` branch does, diverging from
`DoDie`.

## Proposed patch (DEFERRED for blind apply — verify in-game first)

The grep-verified OpenGothic symbols exist: `Npc::onNoHealth`, `physic` (member of `Npc`),
`DynamicWorld::NpcItem::setEnable`. The surgical change is to stop disabling the body on death so a corpse
remains collidable, matching `DoDie`'s `SetPhysicsEnabled(this,1)`:

OLD (`game/world/objects/npc.cpp`, in `Npc::onNoHealth`):
```cpp
  if(death)
    physic.setEnable(false);
```
NEW:
```cpp
  // NOTE: in original-game oCNpc::DoDie @0x00736760 the death handler ends with
  // SetPhysicsEnabled(this,1) — the corpse's collision body stays ENABLED so it can
  // still be hit by projectiles and blocks walk-through; only the movement-collision
  // flag (+0x1b8) is cleared. Do not disable vob physics on death.
  (void)death;
```
and correspondingly drop the `if(isDead()) physic.setEnable(false);` in the load path
(`game/world/objects/npc.cpp:326-327`).

**Why DEFERRED rather than a blind apply:** OpenGothic's capsule physics for a freshly dead NPC may not
fully follow the prone death pose, so leaving the upright capsule enabled could re-introduce an
"invisible standing wall" at the death spot — exactly the artifact this disable likely suppresses. This
must be confirmed in-game (shoot a corpse with a bow; walk into a fresh corpse) before landing. If the
capsule does track the prone pose acceptably, applying the patch restores parity; if not, the correct
parity fix is narrower (keep ray/hit collision enabled for corpses while suppressing the upright
body-overlap), which is a larger change and remains DEFERRED.
