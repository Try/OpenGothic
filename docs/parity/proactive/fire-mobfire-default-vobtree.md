# oCMobFire missing default fireVobtreeName / fireSlot (flames invisible on default-templated fires)

**Confidence:** High

## Original function + address

`oCMobFire::oCMobFire` (Gothic2.exe @0x00722460). The constructor seeds two
member zSTRINGs *before* the vob is unarchived:

- `fireSlot` (member @+0x238) is initialized to `"BIP01 FIRE"`.
- `fireVobtreeName` (member @+0x24c) is initialized to `"FIRETREE_LARGE.ZEN"`.

`oCMobFire::Unarchive` (@0x00722d70) then reads the slot and vobtree strings
with the archiver's ReadString helper (@vtbl+0x88, the keyed-string read used at
+0x234 then +0x248). That helper only overwrites the target when the key is
present in the archive; when a fire vob in a ZEN omits `fireSlot` /
`fireVobtreeName` (or stores them empty), the constructor defaults survive.

`oCMobFire::OnTrigger` (@0x00722980) spawns the fire vobtree when the mob is lit:
it instantiates the vobtree named by `fireVobtreeName`, and as a *second*
fallback, if that string is still empty at trigger time it substitutes
`"FIRETREE_SMALL.ZEN"` (string `s_FIRETREE_SMALL_ZEN`) before spawning. So the
original effectively never spawns an empty/no-flames fire: it has both a ctor
default (FIRETREE_LARGE.ZEN) and an OnTrigger fallback (FIRETREE_SMALL.ZEN).

## OpenGothic file:line

- `game/world/objects/fireplace.cpp:5-9` (ctor) — copies `vob.vob_tree` /
  `vob.slot` verbatim.
- `game/world/objects/fireplace.cpp:23-31` (`onStateChanged`) — builds the
  `VobBundle` from `fireVobtreeName` with no empty-name fallback.
- Upstream: `lib/ZenKit/src/vobs/MovableObject.cc:146-150` — `VFire::load` reads
  `slot` and `vob_tree` via `read_string()`, which yields an **empty** string
  when the archive key is absent; ZenKit applies no `FIRETREE_LARGE.ZEN` /
  `BIP01 FIRE` default.

## Divergence

For an `oCMobFire` whose ZEN entry omits (or empties) `fireVobtreeName`/`fireSlot`
— the common case for fires placed from the default template — OpenGothic ends
up with `fireVobtreeName == ""`. `onStateChanged()` then calls
`VobBundle(world, "", ...)`, which goes through `Resources::implLoadVobBundle("")`
→ `vdfsIndex().find("")` returns `nullptr` → empty bundle (plus a logged
`unable to load Zen-file: ""` error). Result: the campfire/fireplace lights its
state but renders **no flame/smoke vobtree at all**, whereas the original game
shows FIRETREE_LARGE.ZEN (or FIRETREE_SMALL.ZEN). The empty `fireSlot` likewise
mislocates the effect (`mapBone("")`) versus the original `"BIP01 FIRE"` bone.

## Proposed patch

Apply the original constructor defaults when the ZEN leaves the fields empty.
Grep-verified symbols: `FirePlace::fireVobtreeName`, `FirePlace::fireSlot`
(`game/world/objects/fireplace.h:17-18`); `zenkit::VFire::vob_tree`,
`zenkit::VFire::slot` (`lib/ZenKit/include/zenkit/vobs/MovableObject.hh:171,175`).

```cpp
// OLD  (game/world/objects/fireplace.cpp:5-9)
FirePlace::FirePlace(Vob* parent, World& world, const zenkit::VFire& vob, Flags flags)
  : Interactive(parent,world,vob,flags){
  fireVobtreeName = vob.vob_tree;
  fireSlot        = vob.slot;
  }

// NEW
FirePlace::FirePlace(Vob* parent, World& world, const zenkit::VFire& vob, Flags flags)
  : Interactive(parent,world,vob,flags){
  // NOTE: in original-game oCMobFire::oCMobFire (Gothic2.exe @0x00722460) the ctor
  // seeds fireSlot="BIP01 FIRE" and fireVobtreeName="FIRETREE_LARGE.ZEN"; the
  // archiver's keyed ReadString (Unarchive @0x00722d70) leaves those defaults in
  // place when the ZEN omits the keys. ZenKit's VFire::load returns "" instead,
  // so re-apply the defaults here.
  fireVobtreeName = vob.vob_tree;
  fireSlot        = vob.slot;
  if(fireVobtreeName.empty())
    fireVobtreeName = "FIRETREE_LARGE.ZEN";
  if(fireSlot.empty())
    fireSlot = "BIP01 FIRE";
  }
```

(The OnTrigger FIRETREE_SMALL.ZEN fallback is then redundant in practice, since
the ctor default already guarantees a non-empty name; it is intentionally not
ported to avoid second-guessing which template a given fire should use.)
