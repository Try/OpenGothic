# clvl-touch-ignores-disabled — change-level portal fires even when the trigger is disabled

**Confidence:** Medium-High (gate semantics certain and the fix is strictly more faithful / risk-free; only the in-game incidence of a disabled change-level trigger is uncertain)

## Original fn + address

In the original game an `oCTriggerChangeLevel` is an ordinary `zCTrigger` subclass — it does
*not* override `OnTouch`. A player/NPC touch therefore enters `zCTrigger::OnTouch` @0x00610640,
which forwards to `zCTrigger::ActivateTrigger` @0x006104d0 only when the *react-to-on-touch* bit is
set (`flags & 2`). `ActivateTrigger` then calls `zCTrigger::CanBeActivatedNow` @0x00610220, whose
very first test is the *enabled* bit (`field_0x135 & 2`): if the trigger is disabled the function
returns 0 and the activation is dropped — `(oCTriggerChangeLevel::)TriggerTarget` is never reached.
`CanBeActivatedNow` further gates on the remaining-activation counter (`field_0x164`), the
fire-delay timer (`IsOnTimer`), the retrigger cooldown (`field_0x15c`) and the respond-to-type bits
(`respondToPlayer` 0x10 / `respondToNpc` 0x08 / `respondToObject` 0x20).
`oCTriggerChangeLevel::TriggerTarget` @0x0043be20 only runs *after* all those gates pass, and itself
adds the player-only restriction (`param_1 == oCNpc::player`) plus the active-shapeshift/timed-effect
abort (spell ids 0x2f..0x3a -> `EndTimedEffect`, return without `oCGame::ChangeLevel`).

The enabled flag is mutable at runtime: a disabled change-level trigger can come straight from the
`.zen` (`start_enabled == false`) or be flipped by an `OnDisable` / disable trigger-event, and the
original then refuses to change the level on touch.

## OG file:line

`/Users/admin/Downloads/opengothic/game/world/triggers/zonetrigger.cpp:14-26`
(`ZoneTrigger::onIntersect`)

## Divergence

`ZoneTrigger` overrides `onIntersect` wholesale and calls `world.triggerChangeWorld(...)` directly.
Its *only* gate is `n.isPlayer()` (which matches the original's `param_1 == oCNpc::player` test) plus
the transform-back abort. It never consults the trigger's enabled state, so a change-level portal
that the original would treat as inert (`start_enabled == false`, or disabled by a script
`T_Disable`) still teleports the player in OpenGothic. The activation path bypasses
`AbstractTrigger::processEvent` / `CanBeActivatedNow` entirely, so the `disabled` flag maintained by
`AbstractTrigger` (set in the ctor from `start_enabled`, and toggled by `T_Enable`/`T_Disable`) has
no effect on the portal.

The simplest faithful, accessible gate is the enabled check (the public `AbstractTrigger::isEnabled()`
already exists, `abstracttrigger.cpp:59`). Normal portals ship `start_enabled == true`
(`disabled == false`), so the guard is a no-op for them and cannot break ordinary level changes; it
only restores the original behaviour for the disabled case.

(The remaining `CanBeActivatedNow` gates — `reactToOnTouch`, the `respondToPlayer` bit,
`maxActivationCount`, and the retrigger/fire-delay timers — are also bypassed here, but those fields
are `private` in `AbstractTrigger` and for stock G2 change-level vobs are effectively always
permissive, so they are left out of this surgical fix.)

## Proposed patch

OLD (`game/world/triggers/zonetrigger.cpp`):
```cpp
void ZoneTrigger::onIntersect(Npc &n) {
  if(!n.isPlayer())
    return;
  // NOTE: in original-game oCTriggerChangeLevel::TriggerTarget @0x0043be20 the change-level is
  // aborted when the player has an active shapeshift/timed-effect spell (ids 0x2f..0x3a): it ends
  // the timed effect (transformBack) and returns WITHOUT calling oCGame::ChangeLevel. OpenGothic
  // teleported a transformed player straight through the portal, arriving still transformed.
  if(n.isTransformed()) {
    n.transformBack();
    return;
    }
  world.triggerChangeWorld(levelName, startVobName);
  }
```

NEW:
```cpp
void ZoneTrigger::onIntersect(Npc &n) {
  if(!n.isPlayer())
    return;
  // NOTE: in original-game an oCTriggerChangeLevel touch runs through zCTrigger::OnTouch @0x00610640
  // -> zCTrigger::CanBeActivatedNow @0x00610220, whose first gate is the enabled flag
  // (field_0x135 & 2). A disabled change-level trigger (start_enabled==false, or disabled by a
  // T_Disable event) returns 0 there and TriggerTarget @0x0043be20 is never reached, so no
  // ChangeLevel happens. OpenGothic activates the portal inline and ignored the enabled flag,
  // teleporting the player through a disabled portal.
  if(!isEnabled())
    return;
  // NOTE: in original-game oCTriggerChangeLevel::TriggerTarget @0x0043be20 the change-level is
  // aborted when the player has an active shapeshift/timed-effect spell (ids 0x2f..0x3a): it ends
  // the timed effect (transformBack) and returns WITHOUT calling oCGame::ChangeLevel. OpenGothic
  // teleported a transformed player straight through the portal, arriving still transformed.
  if(n.isTransformed()) {
    n.transformBack();
    return;
    }
  world.triggerChangeWorld(levelName, startVobName);
  }
```
