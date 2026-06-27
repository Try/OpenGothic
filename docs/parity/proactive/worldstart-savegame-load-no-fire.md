# zCTriggerWorldStart not re-fired on savegame load

**Confidence:** High

## Original function + address

`zCTriggerWorldStart::PostLoad` @ `0x0061a4e0` is the world-start firing
dispatcher. In prose, it does:

- If the trigger's `fireOnlyFirstTime` flag (object byte at +0x134) is **false**,
  it unconditionally calls the trigger's `OnTrigger(this,this)` (vtable slot
  +0x10) — i.e. it fires on *every* PostLoad pass.
- Otherwise (`fireOnlyFirstTime` true), it fires only when the persistent
  "already-fired" flag (object byte at +0x135) is **0**.

`zCTriggerWorldStart::OnTrigger` @ `0x0061a510` forwards the event to the
trigger target via `zCTriggerBase::OnTrigger` and then sets the +0x135
"already-fired" byte to 1.

The +0x135 byte is significant: `zCTriggerWorldStart::Archive` @ `0x0061a530`
and `Unarchive` @ `0x0061a590` read/write the +0x134 flag unconditionally but
read/write the +0x135 "already-fired" byte **only when the archiver reports
savegame mode** (virtual slot +0x100). Persisting "already-fired" only into
savegames is the proof that `PostLoad` re-runs on savegame loads: the engine
needs the saved flag so that, on resume, a `fireOnlyFirstTime` trigger is *not*
re-fired (its +0x135 is restored to 1) while a `fireOnlyFirstTime==false`
trigger *is* re-fired every load.

Net original behavior on a **savegame load** of the current world:
`fireOnlyFirstTime==false` world-start triggers fire again; `fireOnlyFirstTime`
triggers stay quiet because their saved already-fired flag is restored.

## OpenGothic file:line

- Firing entry point: `game/world/world.cpp:876` `World::triggerOnStart(bool)`
  → `game/world/worldobjects.cpp:520` `WorldObjects::triggerOnStart`.
- New game: `game/game/gamesession.cpp:112` `wrld->triggerOnStart(true)`.
- Level change: `game/game/gamesession.cpp:407`
  `wrld->triggerOnStart(wss.isEmpty())`.
- **Savegame load:** `GameSession::GameSession(Serialize&)` (the deserialize
  ctor, `game/game/gamesession.cpp:116`-onward) loads the world at
  `gamesession.cpp:150` (`wrld->load(fin)`) but **never calls
  `triggerOnStart`**.

## Divergence

When the player loads a savegame (F9 / load menu), OpenGothic's deserialize
constructor restores the world and trigger state but never issues a world-start
pass. Consequently no `TriggerWorldStart` fires on a savegame load. In the
original, `PostLoad` runs on savegame loads and re-fires every
`fireOnlyFirstTime==false` world-start trigger (the "fire on every level
enter / every load" class), while leaving the one-shot
(`fireOnlyFirstTime==true`) ones suppressed via their persisted already-fired
flag. So OpenGothic drops the every-load world-start fire on resume.

The fresh-start and level-change paths are already consistent with the original;
only the direct savegame-load path is missing the call. Because the current
world is always already "started" when resuming, the correct argument is
`firstTime=false`, which routes through `onTrigger` with `ev.type==T_Startup`:
`fireOnlyFirstTime` triggers self-skip (`triggerworldstart.cpp:14`), and
`fireOnlyFirstTime==false` triggers fire — matching the original.

## Proposed patch

`game/game/gamesession.cpp`, in `GameSession::GameSession(Serialize &fin)`,
after the HERO instance is bound (mirroring the level-change ordering at
gamesession.cpp:404-407, which fires after `setInstanceNPC("HERO",...)`):

OLD:
```cpp
  if(auto hero = wrld->player())
    vm->setInstanceNPC("HERO",*hero);

  fin.setEntry("game/camera");
```

NEW:
```cpp
  if(auto hero = wrld->player())
    vm->setInstanceNPC("HERO",*hero);

  // NOTE: in original-game zCTriggerWorldStart::PostLoad @0x0061a4e0 runs on
  // every load, including savegame loads, re-firing every fire_once==false
  // world-start trigger; fire_once triggers stay suppressed via their
  // savegame-persisted already-fired flag (Archive @0x0061a530 writes +0x135
  // only in savegame mode). The current world is already started on resume, so
  // pass firstTime=false: T_Startup re-fires non-one-shot world-start triggers
  // and one-shot ones self-skip in TriggerWorldStart::onTrigger.
  wrld->triggerOnStart(false);

  fin.setEntry("game/camera");
```

Grep-verified symbols: `World::triggerOnStart(bool)`
(`game/world/world.h:127`, `world.cpp:876`); `wrld` member used throughout the
ctor (e.g. `gamesession.cpp:150,166`); `TriggerWorldStart::onTrigger` one-shot
self-skip at `game/world/triggers/triggerworldstart.cpp:14`;
`TriggerEvent::T_Startup` at `game/world/triggers/abstracttrigger.h:24`.
