# Issue #620 — Magic: transformation/rain spells and missing heal VFX

## Issue (umbrella report, multiple symptoms)
1. Transformation spells don't work (turn into Warg, Rat, etc.).
2. Fire rain spell doesn't work.
3. Healing spells lack visual effects.
4. Water-mages restoring ornaments lack visual effects.

This is a broad, partly-stale report. Several pieces are now implemented; the
remainder are separate per-spell coverage gaps, not one bug.

## Subsystem & OG files
- Transformation: `game/world/objects/npc.cpp:99-160` `Npc::TransformBack`,
  `:3237-3245` (transform application: `initializeInstanceNpc`, clear overlays,
  swap level/visual), `:378-390` (save/load of transform), `:323/2687`
  `updateTransform`. So transformation IS wired up.
- Spell cast/effect: `game/game/gamescript.cpp:1147-1169` `invokeSpell`
  (resolves `Spell_Cast_<tag>` script + runs its VisualFX), `npc.cpp:3184`
  `invokeSpell` call, `:3885 beginCastSpell` / `:4040 endCastSpell`.
- Global/area FX (rain-type): `game/game/globaleffects.cpp` (only time/morph/
  screenblend/earthquake are handled as global FX; no precipitation FX path).
- VFX decls: `game/graphics/visualfx.*`, `game/graphics/effect.cpp`.

## Original behavior (Gothic2.exe — prose)
Each spell's behavior is driven by the `Spell_Cast_<NAME>` Daedalus function +
its associated `oCVisualFX` (decl/Play @0x0048a050). Transformation spells call
the engine to re-instance the caster as the target NPC instance and swap the
model; the engine restores the original on expiry. Rain/area spells and heal
spells each `Play` their own oCVisualFX (particle/precipitation/heal glow). The
"missing VFX" symptoms mean specific VisualFX classes (precipitation, heal-glow,
restore-ornament) aren't being instantiated/rendered, not that the cast logic is
absent.

## OpenGothic current behavior
- Transformation: implemented (TransformBack path above) — the original "don't
  work" report predates that or is mod/specific-instance dependent.
- Cast dispatch: generic and present (`invokeSpell`), so any spell with a valid
  `Spell_Cast_*` script runs. Missing visuals therefore point at specific
  VisualFX coverage:
  - No precipitation/rain global-FX path in `globaleffects.cpp` (only 4 global
    FX kinds handled) — fire-rain-style area FX has no engine backing.
  - Heal / restore-ornament glow VFX rely on the particle/VisualFX render path
    in `effect.cpp`; if those specific pfx/vfx decls aren't loaded or the heal
    `on_state`/cast script's `Wld_PlayEffect` target isn't handled, no visual
    shows.

## Divergence
Not a single divergence: an umbrella of per-spell VisualFX gaps. Transformation
and generic cast logic are present; precipitation-style area FX and certain
heal/restore particle FX are the actual missing pieces.

## Disposition: DEFER (umbrella — split required)
Recommend splitting into focused issues, each runtime-verified:
1. Confirm transformation in current build (likely already fixed) — close if OK.
2. Fire-rain: add the missing precipitation/area global-FX path in
   `globaleffects.cpp` + corresponding VisualFX decl handling.
3. Heal/restore VFX: trace the heal `Spell_Cast_*` / `on_state` VisualFX through
   `effect.cpp` and ensure the pfx/vfx decls instantiate and render.
No surgical patch proposed — each sub-item needs in-game observation (cannot be
verified headless) and touches distinct VisualFX/script paths.
