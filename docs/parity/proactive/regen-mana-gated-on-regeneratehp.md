# Mana regen is gated by ATR_REGENERATEHP, not ATR_REGENERATEMANA

**Confidence:** Medium-High. The engine fact is certain (the original's mana-regen
*enable* test reads the **HP** regen field, confirmed twice in two independent reads of
the decompile). Observability is low: it only manifests for an NPC that carries
`ATR_REGENERATEMANA > 0` while `ATR_REGENERATEHP == 0`, which retail scripts rarely
produce. This is a *distinct* divergence from the already-documented reciprocal
rate-vs-period issue (`regen-rate-reciprocal.md`, `heal-regen-rate-inverted.md`,
`sleep-regen-rate-inverted.md`): those concern *how fast* regen runs; this concerns
*whether mana regen runs at all*.

## Original function + address (prose only)

`oCNpc::Regenerate` (Gothic2.exe `@0x00741fd0`, source tag `_ulf/oNpc.cpp`), called once
per living NPC frame. Layout confirmed via `oCNpc::ChangeAttribute` (`@0x0072ff60`):
attribute base `+0x1b8`, so `+0x1b8`=HITPOINTS, `+0x1bc`=…MAX, `+0x1c0`=MANA,
`+0x1c4`=MANAMAX, `+0x1d0`=attr[6]=`ATR_REGENERATEHP`, `+0x1d4`=attr[7]=`ATR_REGENERATEMANA`.

The routine holds two per-NPC float countdown accumulators (`+0x7c4` HP, `+0x7c8` mana),
both decremented each frame by the global frame delta. There are then two independent
increment blocks:

- **HP block:** enable test reads `+0x1d0` (`ATR_REGENERATEHP > 0`); on accumulator
  expiry with HP < HPMAX it adds +1 HP and reloads `+0x7c4` to `attr[ATR_REGENERATEHP]*1000`.
- **Mana block:** the increment itself uses the mana accumulator (`+0x7c8`), the MANA <
  MANAMAX test (`+0x1c0`/`+0x1c4`), adds +1 MANA, and reloads `+0x7c8` to
  `attr[ATR_REGENERATEMANA]*1000` (`+0x1d4`) — **but its enable test reads `+0x1d0`
  (`ATR_REGENERATEHP`), not `+0x1d4`.** So mana only ever regenerates when the *HP* regen
  field is positive; the mana regen field controls only the *interval*, never the
  *enable*. (This is the original-game quirk noted in passing in
  `sleep-regen-rate-inverted.md`; here it is the primary subject.)

Consequence in the original:
- `REGENERATEHP == 0`, `REGENERATEMANA > 0` → **no mana regen** (enable test fails).
- `REGENERATEHP > 0`, `REGENERATEMANA == 0` → mana reload becomes `0*1000 = 0`, so the
  accumulator re-expires every frame: +1 mana **per frame** up to MANAMAX (pathological
  runaway; not realistic in retail data and not proposed for replication below).

## OpenGothic file:line

`game/world/objects/npc.cpp:2502-2503` — `Npc::tick` calls
`tickRegen(... hnpc->attribute[ATR_REGENERATEMANA], dt)` unconditionally inside the
`if(!isDead())` block. `Npc::tickRegen` (`npc.cpp:2424`) early-returns only on `chg==0`,
so it treats `ATR_REGENERATEMANA` as the sole enable/gate for mana regen, independent of
`ATR_REGENERATEHP`.

## Divergence

For an NPC with `ATR_REGENERATEHP == 0` and `ATR_REGENERATEMANA > 0`: the original
performs **no** mana regen, OpenGothic regenerates mana. The gate attribute differs
(`ATR_REGENERATEHP` in the engine vs `ATR_REGENERATEMANA` in OpenGothic).

## Proposed patch (gate the mana call on ATR_REGENERATEHP)

OLD (`game/world/objects/npc.cpp:2499-2504`):
```cpp
  if(!isDead()) {
    tickRegen(hnpc->attribute[ATR_HITPOINTS],hnpc->attribute[ATR_HITPOINTSMAX],
              hnpc->attribute[ATR_REGENERATEHP],dt);
    tickRegen(hnpc->attribute[ATR_MANA],hnpc->attribute[ATR_MANAMAX],
              hnpc->attribute[ATR_REGENERATEMANA],dt);
    }
```
NEW:
```cpp
  if(!isDead()) {
    tickRegen(hnpc->attribute[ATR_HITPOINTS],hnpc->attribute[ATR_HITPOINTSMAX],
              hnpc->attribute[ATR_REGENERATEHP],dt);
    // NOTE: in original-game oCNpc::Regenerate @0x00741fd0 the mana-regen *enable* test
    // reads ATR_REGENERATEHP (struct +0x1d0), not ATR_REGENERATEMANA (+0x1d4); the mana
    // field only sets the interval. So mana never regenerates while ATR_REGENERATEHP==0.
    if(hnpc->attribute[ATR_REGENERATEHP]>0)
      tickRegen(hnpc->attribute[ATR_MANA],hnpc->attribute[ATR_MANAMAX],
                hnpc->attribute[ATR_REGENERATEMANA],dt);
    }
```
Symbols grep-verified: `ATR_REGENERATEHP` (constants.h:479), `ATR_REGENERATEMANA`
(constants.h:480), `ATR_MANA` (constants.h:475), `Npc::tickRegen` (npc.cpp:2424).

The `REGENERATEHP>0 & REGENERATEMANA==0` per-frame runaway is **not** replicated (OG's
`chg==0` early-return suppresses it); that edge is pathological and absent from retail
data, so leaving it as-is is the safer parity choice.

## DEFERRED recommendation

Apply only **together with** the reciprocal rate-vs-period decision tracked in
`regen-rate-reciprocal.md` / `heal-regen-rate-inverted.md`: while OpenGothic's base regen
*rate* formula is still the reciprocal of the engine's, fixing the mana *enable* gate in
isolation changes mana behavior in a way that is hard to validate against the original
end-to-end. The gate fix above is correct and surgical, but its observable benefit is
gated on the same low-frequency data condition the sibling docs are blocked on, so it is
filed as a DEFERRED companion fix rather than an independent change.
