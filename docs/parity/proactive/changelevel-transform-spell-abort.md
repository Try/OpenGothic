# Change-Level trigger ignores active shapeshift/transform spell (no abort, no transform-back)

**Confidence:** High (clear, observable, maps to existing OpenGothic symbols)

## Original function + address
`oCTriggerChangeLevel::TriggerTarget` @ `0x0043be20`.

After calling the base `zCTrigger::TriggerTarget`, the function only proceeds when the
triggering vob is the player (`param_1 == oCNpc::player`). Before it ever reaches the world
swap it runs a guard loop over spell ids `0x2f..0x3a` (47..58 — the shapeshift / timed-effect
"transform" spells): for the first one that `oCNpc::IsSpellActive` reports active, it calls
`oCSpell::EndTimedEffect(...)` and then **returns immediately** — i.e. it cancels the
transformation and does NOT change level. Only when no such spell is active does it fall through
to the cleanup (kill the selected spell / force-remove weapon when in spell weapon-mode 7, then
`oCNpc::KillActiveSpells(player)`) and finally invoke `oCGame::ChangeLevel(levelName, startVob)`
(`0x006c7290`, dispatched through the `ogame` vtable slot at `+0x88`).

Net player-visible behavior in the original: stepping into a level-change zone while transformed
into an animal (bloodfly, fire golem, wolf, etc.) does NOT teleport you to the other level. It
silently ends the transformation and leaves you standing on the trigger; you must transform back
on your own and re-enter the zone to actually change level.

## OpenGothic file:line
`game/world/triggers/zonetrigger.cpp:13-16`

```cpp
void ZoneTrigger::onIntersect(Npc &n) {
  if(n.isPlayer())
    world.triggerChangeWorld(levelName, startVobName);
  }
```

## Divergence
OpenGothic performs the world transition unconditionally as soon as the player touches the zone.
It never consults the player's transform state, so a transformed player walks straight through the
portal and arrives in the destination world still transformed. The original aborts the transition
and cancels the shapeshift first. OpenGothic already models exactly this state: `Npc::transformSpl`
(`game/world/objects/npc.h:601`) is non-null iff the player is under a transform spell from that
same id range, and `Npc::transformBack()` (`game/world/objects/npc.cpp:4730`, public decl
`game/world/objects/npc.h:331`) is the OpenGothic equivalent of `EndTimedEffect` undoing the
transform.

## Proposed patch
`transformSpl` is private, so expose a const query next to `transformBack()` and gate the
transition on it.

`game/world/objects/npc.h` (near line 331):
```cpp
// OLD
    void      transformBack();
// NEW
    void      transformBack();
    bool      isTransformed() const { return transformSpl!=nullptr; }
```

`game/world/triggers/zonetrigger.cpp`:
```cpp
// OLD
void ZoneTrigger::onIntersect(Npc &n) {
  if(n.isPlayer())
    world.triggerChangeWorld(levelName, startVobName);
  }
// NEW
void ZoneTrigger::onIntersect(Npc &n) {
  if(!n.isPlayer())
    return;
  // NOTE: in original-game oCTriggerChangeLevel::TriggerTarget @0x0043be20 the change-level is
  // aborted when the player has an active shapeshift/timed-effect spell (ids 0x2f..0x3a): it
  // ends the timed effect (transformBack) and returns without calling oCGame::ChangeLevel.
  if(n.isTransformed()) {
    n.transformBack();
    return;
    }
  world.triggerChangeWorld(levelName, startVobName);
  }
```

Grep-verified symbols: `ZoneTrigger::onIntersect` / `levelName` / `startVobName`
(`game/world/triggers/zonetrigger.*`), `Npc::isPlayer`, `Npc::transformBack`, `Npc::transformSpl`
(`game/world/objects/npc.*`).

Scope note: the original also force-removes a drawn spell/weapon and runs `KillActiveSpells`
before the swap (the `weaponMode==7` / `oCNpc::KillActiveSpells` cleanup). That secondary cleanup
is intentionally left out of this surgical patch — it is lower-observability and would need its
own symbol verification; this fix targets only the high-confidence transform-abort gate.
