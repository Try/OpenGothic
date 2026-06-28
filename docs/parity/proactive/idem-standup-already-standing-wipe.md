# AI_StandUp on an already-standing NPC wipes all anims + pfx effects instead of no-op'ing

**Confidence:** Medium-High (the original idempotency guard is High-confidence from the
decompile; the only uncertainty is the layer-fade nuance — original fades gesture layers 2-8
where the proposed fix simply leaves them — noted below).

## Original function + address

`AI_StandUp` routes `AI_StandUp` external -> `oCNpc::EV_StandUp` (Gothic2.exe @ `0x00683ce0`)
-> `oCNpc::StandUp` (@ `0x00682b40`) on the first message frame.

The body of `StandUp` that resets body-state and (re)starts the locomotion/idle animation is
gated by an explicit **already-standing early-out**. For a non-interacting NPC it reads, in
effect:

```
if (interactMob == 0 && field(0x968) == 0) {
    if (oCAniCtrl_Human::IsStanding(anictrl) == 0) {   // ONLY when NOT already standing
        ... SetBodyState(BS_STAND); StopTurnAnis(); StartAni(idle/swim/dive) ...
    }
    // IsStanding != 0  -> the whole SetBodyState/StartAni block is skipped
}
...
RbtReset(this);
if (model) zCModel::FadeOutAnisLayerRange(model, 2, 8);   // fade gesture/overlay layers only
... weapon-relax morph ...
```

So when the NPC is already standing (`IsStanding() != 0`), `StandUp` does **not** restart the
base locomotion ani, does **not** re-issue `SetBodyState`, and does **not** stop the active
animations or clear effects. The only thing it does for an already-standing NPC is
`RbtReset` + `FadeOutAnisLayerRange(2,8)` (a graceful fade of the upper gesture/overlay layers,
leaving the base loco layers 0/1 and pfx effects untouched). The standup is idempotent on a
standing NPC.

This is the same idempotency class as `oCNpc::DropUnconscious` (@ `0x00735eb0`) early-returning
on `IsInState(-4)`.

## OpenGothic file:line

`game/world/objects/npc.cpp:2787-2791` (`Npc::nextAiAction`, general standup branch of
`case AI_StandUp` / `case AI_StandUpQuick`).

## Divergence

OpenGothic has no already-standing guard: every non-dead, non-unconscious, non-lie NPC
(which includes `bs == BS_STAND`) falls into the general branch and runs the full reset:

```cpp
else if(bs!=BS_DEAD) {
  visual.stopAnim(*this,"");          // Pose::stopAnim("") stops ALL layers AND effects.clear()
  setStateItem(MeshObjects::Mesh(),"");
  setAnim(Anim::Idle);                // forces base loco back to plain Idle
  }
```

`MdlVisual::stopAnim(npc,"")` (mdlvisual.cpp:576) calls `skInst->stopAnim("")` (stops every
animation layer) and, because the name is empty, `effects.clear()` (drops every attached pfx
effect), then re-seeds `Idle`. So re-issuing `AI_StandUp` on an **already-standing** NPC in
OpenGothic wholesale wipes its active animation layers and all pfx effects and restarts the
base Idle ani — whereas the original keeps the base ani and effects and only fades the gesture
layers.

This path is hit constantly: `B_ASSESSTALK` issues `AI_StandUp` on essentially every NPC that
is greeted, and most greeted NPCs are already standing. The original treats that as a no-op
(plus a gentle gesture-layer fade); OpenGothic instead interrupts whatever the standing NPC was
doing and clears its ambient/continuous pfx effects each time it is talked to.

## Proposed patch

`game/world/objects/npc.cpp`, general standup branch:

```cpp
// OLD
      else if(bs!=BS_DEAD) {
        visual.stopAnim(*this,"");
        setStateItem(MeshObjects::Mesh(),"");
        setAnim(Anim::Idle);
        }

// NEW
      else if(bs!=BS_DEAD && bs!=BS_STAND) {
        // NOTE: in original-game oCNpc::StandUp (Gothic2.exe @0x00682b40) the body-state reset
        // and StartAni(idle) are gated by oCAniCtrl_Human::IsStanding()==0, i.e. they run ONLY
        // when the NPC is NOT already standing; for an already-standing NPC StandUp is a no-op
        // (it neither restarts the base ani nor clears effects, it only fades the gesture
        // layers). OpenGothic's stopAnim("") also clears all pfx effects, so re-issuing
        // AI_StandUp on a standing NPC (B_ASSESSTALK fires it on nearly every greeted NPC) wiped
        // its animations and ambient effects. Skip the reset when already standing.
        visual.stopAnim(*this,"");
        setStateItem(MeshObjects::Mesh(),"");
        setAnim(Anim::Idle);
        }
```

Nuance (non-regression): the original additionally calls `FadeOutAnisLayerRange(2,8)` on the
already-standing path, gently fading upper gesture/overlay layers. The patch simply leaves those
layers running rather than fading them — a strictly smaller change than the current
wipe-everything behaviour and faithful to the dominant original semantics (base ani and effects
preserved). If exact gesture-fade parity is later wanted, that is a separate, additive follow-up.
