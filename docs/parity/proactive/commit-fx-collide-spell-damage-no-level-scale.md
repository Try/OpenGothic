# commit: non-projectile spell FX-collision damage is not scaled by spell level

**Confidence:** Medium-High (divergence verified against disassembly; fix is non-surgical → DEFERRED).

## Original fn + address (prose)

Engine-side spell damage is produced by `oCVisualFX::ProcessCollision` (Gothic2.exe
`0x004958d0`). When a spell FX collides with an NPC, ProcessCollision builds the per-hit
damage total before calling `ApplyDamages` (`0x0065e5a0`). The total is computed as
`GetDamage() * GetLevel() * factor`:

- vtable slot `+0xf4` → `oCVisualFX::GetDamage` (`0x0048a140`, returns field `0x560`, the
  `C_Spell.damage_per_level` value that `oCSpell::CreateEffect @0x004842e0` wrote into the FX
  via `SetDamage` at FX-creation time);
- vtable slot `+0xa4` → `oCVisualFX::GetLevel` (`0x00493150`, the invested spell level);
- a 0.5/1.0/2.0 collision `factor` (ProcessCollision local, `poStack_144 & 4 / & 8`).

(Vtable mapping confirmed numerically: `oCVisualFX` vtable base `0x0083029c`; `+0xf4`→GetDamage
`0x00830390`, `+0xa4`→GetLevel `0x00830340`, `+0xf8`→GetDamageType.) `ApplyDamages` then splits
that level-scaled total equally across the spell's damage-type bits
(`round(total/numTypes)`), the same split already mirrored elsewhere.

So the original deals `damage_per_level * level` (further split per type), i.e. damage grows
with the cast level for **every** spell that does damage through FX collision — projectiles
*and* non-projectile/instant collision spells.

## OG file:line

- `game/world/objects/npc.cpp:2166-2181` — `Npc::takeDamage(...,isSpell=true)` computes the
  spell damage as `perType = round(spl.damage_per_level / numTypes)`; **no `* level`**.
- `game/game/damagecalculator.cpp:18-22` — `damageValue` uses the bullet's pre-scaled damage
  when `b != nullptr` (`rangeDamage(...,*b,...)`), but uses the `takeDamage`-computed `splDmg`
  when `b == nullptr` (`rangeDamage(...,splDmg,...)`).
- `game/graphics/effect.cpp:374-376` — the FX physics-collision lambda calls
  `npc.takeDamage(*src, b /* ==nullptr for a non-projectile FX */, vfx, sId)`.
- `game/world/objects/npc.cpp:3398-3437` — `commitSpell`: the *projectile* branch
  (`isSpellShoot()`) builds `damage_per_level * lvl` and stores it on the bullet via
  `b.setDamage(dmg)` (line ~3410/3417); the *else* (non-projectile FX) branch spawns the FX
  with `e.setSpellId(splId,owner)` only — **no level is ever attached to the FX**.

## Divergence

Projectile spells are correct in OG: `commitSpell` pre-computes `damage_per_level * lvl`,
stores it on the `Bullet`, and `damageValue` uses the bullet damage (`b != nullptr` branch),
so projectile damage scales with level.

Non-projectile spells that deal damage through FX collision take the `b == nullptr` path:
`Effect::setupCollision`'s lambda → `Npc::takeDamage(*src, nullptr, vfx, splId)` →
`damageValue` selects `splDmg`, which `takeDamage` computed as `damage_per_level / numTypes`
with **no level factor**. These spells therefore always deal their *level-1* damage in
OpenGothic, regardless of how many mana levels were invested, whereas the original multiplies
by `GetLevel()`. (The original also applies the 0.5/1/2 collision factor, likewise absent.)

The root cause is structural: OpenGothic never plumbs the cast level into the launched FX.
The `Effect` carries only `splId`, and `takeDamage` has no level parameter, so the FX-collision
damage path has no way to recover the level — unlike the projectile path, which bakes the
level-scaled damage into the `Bullet` at `commitSpell` time.

## Proposed patch

**DEFERRED.**

Reason: there is no surgical one-line fix. `Npc::takeDamage(Npc&,const Bullet*,const VisualFx*,
int32_t)` and the `damagecalculator` spell path receive no spell level, and the colliding
`Effect` does not store one. A correct fix requires plumbing the invested cast level from
`commitSpell`'s non-projectile branch into the launched `Effect` (a new `Effect` level member +
setter, propagated through the `next`-chain and through `Effect::onCollide` /
`Effect::setupCollision`'s captured lambda) and then into `Npc::takeDamage` so the
`damage_per_level` spell-damage computation at `npc.cpp:2177` can multiply by that level. That
touches `game/graphics/effect.{h,cpp}`, `game/world/objects/npc.{h,cpp}`, and possibly
`game/game/damagecalculator.cpp`, and must avoid disturbing the already-correct projectile
(`b != nullptr`) branch. Deferred to a dedicated surgical pass rather than risk a half-plumbed
change.

Citation for the eventual fix:
`// NOTE: in original-game oCVisualFX::ProcessCollision @0x004958d0 the spell hit total is`
`// GetDamage()(=C_Spell.damage_per_level) * GetLevel()(invested level) [* 0.5/1/2 factor],`
`// then split per damage-type by ApplyDamages @0x0065e5a0. OpenGothic's b==nullptr FX-collision`
`// path applied damage_per_level without the level multiplier (level-1 damage only).`
