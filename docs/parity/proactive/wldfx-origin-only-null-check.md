# Wld_PlayEffect requires a matching-type target; original only requires a valid origin

**Confidence:** Medium-High (logic divergence is certain from the binary; in-game impact frequency not measured)

## Original function + address (prose)

The `Wld_PlayEffect` external lives at `Gothic2.exe` `0x006dfc20`
(`P:\dev\g2addon\release\Gothic\_ulf\oGameExternal.cpp`). It pops the four
trailing integers (`isProjectile`, `damageType`, `damage`, `effectLevel`), then
fetches two script instances and `RTDynamicCast`s each to `zCVob` — the first is
the *target* vob, the second is the *origin* (source) vob. It then null-checks
**only the origin**: if the origin is null it reports
`"C: Wld_PlayEffect: Origin Script Instance is NULL!"` and returns without doing
anything. Otherwise it pops the visual name and calls
`oCVisualFX::CreateAndPlay(name, origin, target, effectLevel, (float)damage, damageType, isProjectile)`
(the vob overload at `0x0048e760`), releasing the returned FX reference.

Two facts follow directly from that decompile:
- The **target** instance is optional. It may be null (a null cast result is
  passed straight through to `CreateAndPlay`), and it is never type-restricted —
  origin and target are plain `zCVob*`, so either can be an NPC, an item, a MOB,
  or any other vob.
- The effect is **always** created and played for a valid origin; the four
  integer parameters are forwarded to the FX, not used to suppress it.

## OpenGothic file:line

`/Users/admin/Downloads/opengothic/game/game/gamescript.cpp:1701-1714`
(`GameScript::wld_playeffect`).

## Divergence

```cpp
auto dstNpc = findNpcById(targetId);
auto srcNpc = findNpcById(sourceId);
auto dstItm = findItemById(targetId);
auto srcItm = findItemById(sourceId);

if(srcNpc!=nullptr && dstNpc!=nullptr) {
  srcNpc->startEffect(*dstNpc,*vfx);
  } else
if(srcItm!=nullptr && dstItm!=nullptr){
  Effect e(*vfx,world(),srcItm->position());
  e.setActive(true);
  world().runEffect(std::move(e));
  }
```

OpenGothic resolves *both* operands to a concrete type and plays the effect only
when source and target are **both NPCs** or **both items** (and both non-null).
Every other combination that the original handles is silently dropped:
- source NPC + **null/none** target,
- source item + **null/none** target,
- mixed source/target (NPC source with item target, or vice-versa).

In the original each of these still plays the effect, because only the *origin*
is required. OpenGothic's mapping of the handled case is otherwise correct
(`srcNpc->startEffect(*dstNpc,...)` puts origin=source, target=dstNpc, matching
the original argument order), so this is a missing-case divergence rather than a
swapped-argument bug.

Note: the earlier `if(isProjectile!=0 || damageType!=0 || damage!=0 || effectLevel!=0)`
TODO bail-out (gamescript.cpp:1690) still short-circuits any call that carries
damage/level/projectile flags, so this fix only affects all-zero
`Wld_PlayEffect` calls — but for those it brings OpenGothic in line with the
original "origin-only" requirement.

## Proposed patch

Drop the target-type requirement so a valid origin alone is sufficient; when the
target is absent or not an NPC, default it to the source (the same self-default
the spell-cast path already uses at `npc.cpp:3368`,
`e.setTarget((currentTarget==nullptr) ? this : currentTarget)`). This strictly
broadens behaviour — every combination currently handled stays byte-for-byte
identical.

OLD (gamescript.cpp:1701):
```cpp
  auto dstNpc = findNpcById(targetId);
  auto srcNpc = findNpcById(sourceId);

  auto dstItm = findItemById(targetId);
  auto srcItm = findItemById(sourceId);

  if(srcNpc!=nullptr && dstNpc!=nullptr) {
    srcNpc->startEffect(*dstNpc,*vfx);
    } else
  if(srcItm!=nullptr && dstItm!=nullptr){
    Effect e(*vfx,world(),srcItm->position());
    e.setActive(true);
    world().runEffect(std::move(e));
    }
```

NEW:
```cpp
  auto dstNpc = findNpcById(targetId);
  auto srcNpc = findNpcById(sourceId);
  auto srcItm = findItemById(sourceId);

  // NOTE: in original-game Wld_PlayEffect @0x006dfc20 only the *origin* (source)
  // instance is null-checked ("Origin Script Instance is NULL!"); the target is
  // optional and untyped, and the effect plays for any valid origin vob.
  if(srcNpc!=nullptr) {
    srcNpc->startEffect((dstNpc!=nullptr) ? *dstNpc : *srcNpc, *vfx);
    } else
  if(srcItm!=nullptr){
    Effect e(*vfx,world(),srcItm->position());
    e.setActive(true);
    world().runEffect(std::move(e));
    }
```

Residual limitation (left as-is, matches a separate gap): origins that are
neither an NPC nor an item (MOBs, decoration/marker vobs) still cannot be
resolved by `findNpcById`/`findItemById`, so those origins remain unhandled —
unchanged from current behaviour.

Symbols verified to exist: `findNpcById`/`findItemById`
(gamescript.h:213-219), `Npc::startEffect(Npc&,const VisualFx&)`
(npc.h:421 / npc.cpp:3275), `Effect(const VisualFx&,World&,const Vec3&)`
(effect.h:25), `World::runEffect` (world.h:92), `Item::position()`
(item.h:57).
