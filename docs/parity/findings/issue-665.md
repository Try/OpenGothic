# Issue #665 — `bloodDetail` param has no effect

**Category:** config/VFX · **Disposition:** DEFER (needs new VFX path)

## Intended behavior (original)
`[GAME] bloodDetail` (0..3) controls blood particle effects when an NPC is hit /
at low health: blood spatters in the air and decals/puddles on the ground, with
amount scaling by the value. Per-guild blood is described in `C_GILVALUES`
(`blood_disabled`, `blood_amount`, `blood_flow`, `blood_emitter`,
`blood_texture`, `blood_max_distance`).

## OpenGothic — current state
- `bloodDetail` is NOT read anywhere (`grep bloodDetail game/ lib/` → no hits).
- Guild blood fields ARE loaded into ZenKit structs:
  - definitions: `lib/ZenKit/include/zenkit/addon/daedalus.hh:49-53`
  - registration: `lib/ZenKit/src/addon/daedalus.cc:69-73`
  - copied to derived guilds in `game/game/gamescript.cpp:419-424`
    (`cGuildVal->blood_emitter[i] = …`, etc.).
- BUT these values are never consumed at runtime: no blood emitter is spawned on
  damage. `game/game/damagecalculator.cpp` reads `guildVal()` (l.40) for combat
  math only. The combat-PFX hook in `game/world/world.cpp:781-788` builds
  `CPFX_<scheme>_<src>_<dst>` weapon-impact effects (metal/wood/stone) — there
  is no `blood_emitter` PFX spawn keyed off the victim's guild.

## Gap
Blood VFX is entirely unimplemented. `bloodDetail` is a quality knob for a
feature that does not exist yet, so the ini value is correctly ignored.

Maintainer confirmed in-thread (Aug 2024): *"Blood is not implemented yet, so
bloodDetail has no effect"*; (Jan 2025) it is *"not on top of things to do."*

## Recommendation
DEFER. This is a new rendering/VFX feature, not surgical config plumbing:
1. On a successful damaging hit, look up the victim's guild blood fields
   (`blood_disabled`, `blood_emitter`, `blood_amount`).
2. Spawn the `blood_emitter` PFX at the hit location (reuse the PfxEmitter path
   used for `CPFX_*`), scaling particle count by `GAME/bloodDetail`.
3. Optionally drop a ground decal/puddle for `bloodDetail>=2`.
Read `bloodDetail` once via `Gothic::settingsGetI("GAME","bloodDetail",2)` and
gate the emitter on it. No safe one-line patch closes this issue.
