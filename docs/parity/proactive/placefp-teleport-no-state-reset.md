# AI_Teleport places the NPC but never resets its in-progress action/animation

**Confidence:** Medium (divergence itself: High; exact surgical patch shape: Medium)

## Original function + address (prose only)

The Daedalus external `AI_Teleport` is handled by the game-external thunk at
`FUN_006de400` (`oGameExternal.cpp`). It reads the target way/free-point name,
and for a non-player NPC whose event-message (EM) queue is currently empty it
calls `oCNpc::BeamTo` (`0x00736ee0`, `oNpc.cpp`) **immediately**; otherwise it
posts an `oCMsgMovement` of subtype `0x10` (EV_Teleport) onto the NPC's EM queue,
which is later dispatched by `oCNpc::OnMessage` (`0x0074b020`) — and that path
*also* calls `oCNpc::BeamTo`. So every `AI_Teleport`, queued or immediate,
ultimately runs `BeamTo`.

`oCNpc::BeamTo` resolves the target. For a way-point it takes the position from
`zCWaypoint::GetPositionWorld` and the heading from the way-point direction
field; for a free-point it takes the trafo translation and the spot vob's
forward ("at") column. It then does `SetCollDet* off`, `SetPositionWorld`,
`SetHeadingAtWorld`, `SetCollDet* on`, and crucially calls the virtual
`oCNpc::ResetPos` (`0x006824d0`, `oNpc_Move.cpp`) with the new position, finally
`oCSpawnManager::SpawnImmediately`.

`oCNpc::ResetPos` is the state-reset step: it calls `Interrupt`, stops the model
animation layers (`zCModel::StopAnisLayerRange`) and restarts the default/idle
ani, resets the human ani-controller (`oCAniCtrl_Human::Reset` + action mode),
`ClearEM`, `SetBodyState(0)`, `SetMovLock(0)`, and re-seats the rigid body. Net
effect in the original: a teleported NPC is hard-snapped to the target **and**
its current locomotion/attack/interaction action is interrupted and it returns
to idle at the destination.

## OpenGothic file:line

`game/world/objects/npc.cpp:2804-2812` (the `AI_Teleport` case inside
`Npc::nextAiAction`):

```cpp
case AI_Teleport: {
  setPosition (act.point->position() );
  setDirection(act.point->direction());
  if(isPlayer()) {
    updateTransform();
    Gothic::inst().camera()->reset(this);
    }
  }
  break;
```

## Divergence

OpenGothic moves the NPC's position and heading but performs **no** interrupt /
state reset. Any in-progress go-to (`go2`)/walk, attack, or playing animation
keeps running after the teleport: the NPC slides toward its old movement target
or finishes its prior animation from the new location, instead of being snapped
to idle as the original `BeamTo`→`ResetPos` does. The player-only branch only
fixes the camera; it does not interrupt locomotion either. This is the
"teleport keeps/resets state" behavior — original resets, OpenGothic keeps.

## Proposed patch

Mirror the visible, low-risk subset of `ResetPos` (interrupt current locomotion
and return to idle). Both symbols are grep-verified members of `Npc`
(`game/world/objects/npc.h:384` `clearGoTo`, `:385` `stopWalking`; `Anim::Idle`
used at `npc.cpp:1019,1631,1808,2246`). `clearGoTo()` already calls
`stopWalking()` internally.

OLD:
```cpp
    case AI_Teleport: {
      setPosition (act.point->position() );
      setDirection(act.point->direction());
      if(isPlayer()) {
        updateTransform();
        Gothic::inst().camera()->reset(this);
        }
      }
      break;
```

NEW:
```cpp
    case AI_Teleport: {
      setPosition (act.point->position() );
      setDirection(act.point->direction());
      // NOTE: in original-game AI_Teleport (FUN_006de400) always runs
      // oCNpc::BeamTo @0x00736ee0, which after SetPositionWorld/SetHeadingAtWorld
      // calls the virtual oCNpc::ResetPos @0x006824d0 -> Interrupt + stop ani
      // layers + restart idle + SetMovLock(0). Interrupt the in-progress
      // locomotion/animation so a teleported NPC does not keep sliding/playing
      // its previous action at the destination.
      clearGoTo();
      setAnim(Npc::Anim::Idle);
      if(isPlayer()) {
        updateTransform();
        Gothic::inst().camera()->reset(this);
        }
      }
      break;
```

### Deferred remainder
Full `ResetPos` parity (`ClearEM` of the per-frame movement message queue,
`SetBodyState(0)`, ani-controller reset, rigid-body re-seat, `SpawnImmediately`)
is **DEFERRED**: those touch the body-state / EM / physics subsystems with no
isolated OpenGothic equivalent, and reproducing them is not a surgical,
regression-safe change. The patch above covers the observable locomotion/
animation interrupt only; the position+heading half already matches the
original (raw way-point world position + way-point/free-point direction,
horizontal heading), so no positional change is proposed.
