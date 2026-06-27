# Sound-vob triggered-on state is not persisted across save/load

**Confidence:** High (divergence). Medium (patch — touches three files and adds a new
serialization stream, so it needs a clean rebuild + save-roundtrip test).

## Original function + address (prose only)

`zCVobSound::Archive` (Gothic2.exe @0x0063e3d0). After the normal property block it opens
the savegame-only branch (the archiver `InSaveGame` slot, vtable +0x100). Inside that branch
it persists two runtime bits of the object's flag byte (`field_0x140`):

- bit5 (mask 0x20) — the **triggered-on** state. `zCVobSound::OnTrigger` @0x0063e350 sets it
  (`flags |= 0x20`) and calls `StartSound`; `zCVobSound::OnUntrigger` @0x0063e370 clears it
  (`flags &= 0xDF`). This is the runtime "a trigger has switched this sound vob on" flag.
- bit1 (mask 0x02) — the "currently has a live sound handle" flag, cleared/stopped in
  `OnUntrigger`.

`zCVobSound::Unarchive` @0x0063e540 reads both bits back in its savegame branch, so a sound
vob that a trigger turned on (e.g. an ambient loop a quest enables) stays on after load in the
original. The non-runtime properties (`initially_playing`/`sndStartOn`, mode, volume, radius)
live in the always-written property block.

## OpenGothic file:line

- `game/world/worldsound.cpp:230` — `WorldSound::execTriggerEvent` sets `worldEff[i].active = true`
  (the OpenGothic equivalent of `OnTrigger` setting bit5).
- `game/world/worldsound.cpp:90` (`addSound`) — `active` is seeded from `vob.initially_playing`
  and is the only thing that decides whether the loop plays (`worldsound.cpp:171-172`).
- `WorldSound` has **no** `save`/`load` method, and `World::save`/`World::load`
  (`game/world/world.cpp:188`/`:170`) never serialize it. The only save/load streams under
  `world/` are `world.cpp` and `worldobjects.cpp`.

## Divergence

`WorldSound::WSound::active` is pure runtime state that is mutated by trigger events
(`execTriggerEvent` → `active=true`) and by the daytime-window logic (`worldsound.cpp:220`
→ `active=false`). It is never written to the savegame. On load the world is rebuilt from the
`.zen` and every sound vob's `active` reverts to its `initially_playing` default. So a
`zCVobSound`/`zCVobSoundDaytime` that a scripted trigger switched **on** (whose vob default is
off) goes silent after a save/load, and one switched **off** comes back on — whereas
`Gothic2.exe` restores the persisted `field_0x140` bit5 and keeps the runtime state. This is an
audible parity break in the "sound-vob state save" subsystem.

## Proposed patch

Persist `WSound::active` keyed by `vobName` (deterministic and order-independent vs. the
re-parsed `.zen`; vob names in a world are effectively unique for sound vobs). Guard with a new
version and bump `Serialize::Current`.

`game/game/serialize.h` — OLD:
```cpp
      Current    = 57, // 57: persist MoveTrigger::triggerCount (TRIGGER_CONTROL ref count)
```
NEW:
```cpp
      Current    = 58, // 58: persist WorldSound vob-sound active/triggered-on state
      // 57: persist MoveTrigger::triggerCount (TRIGGER_CONTROL ref count)
```

`game/world/worldsound.h` — add to the public section (next to `execTriggerEvent`):
```cpp
    void    save(Serialize& fout) const;
    void    load(Serialize& fin);
```

`game/world/worldsound.cpp` — add (uses the existing `worldEff`/`WSound::vobName`/`WSound::active`
members, all grep-verified):
```cpp
// NOTE: in original-game zCVobSound::Archive @0x0063e3d0 the savegame branch persists the
// runtime triggered-on bit (field_0x140 bit5, set in OnTrigger @0x0063e350 / cleared in
// OnUntrigger @0x0063e370). OpenGothic rebuilt sound vobs from the .zen on load, dropping any
// trigger-driven on/off change. Persist WSound::active to match.
void WorldSound::save(Serialize& fout) const {
  fout.setEntry("worlds/",fout.worldName(),"/sound");
  fout.write(uint32_t(worldEff.size()));
  for(auto& i:worldEff)
    fout.write(i.vobName,i.active);
  }

void WorldSound::load(Serialize& fin) {
  if(fin.version()<58)
    return;
  if(!fin.setEntry("worlds/",fin.worldName(),"/sound"))
    return;
  uint32_t sz=0;
  fin.read(sz);
  for(uint32_t n=0;n<sz;++n) {
    std::string name; bool active=false;
    fin.read(name,active);
    for(auto& i:worldEff)
      if(i.vobName==name)
        i.active = active;
    }
  }
```

`game/world/world.cpp` — wire into the existing world stream (the `wsound` member is
`World::wsound`, exposed via `sound()`):
- in `World::save` after `wobj.save(fout);` add `wsound.save(fout);`
- in `World::load` after `wobj.load(fin);` add `wsound.load(fin);`

Verification notes for the implementer: `WSound::vobName` and `WSound::active` already exist
(`worldsound.cpp:26,31,92,94`); `worldEff` is `std::vector<WSound>` (`worldsound.h:86`);
`Serialize::write/read/setEntry/version/worldName` are the same helpers used by
`WorldObjects::save`/`load`. Build and roundtrip a savegame with a trigger that enables an
`initially_playing=false` sound vob, confirm it keeps playing after load.
