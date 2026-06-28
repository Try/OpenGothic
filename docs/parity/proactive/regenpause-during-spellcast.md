# Regen paused during spell-casting (cast early-return skips Regenerate)

**Confidence:** Medium-High

## Original fn + address

`oCNpc::Regenerate` @ `0x00741fd0` is the per-tick HP/mana regeneration routine.
Its *only* entry guard on the attribute-regen block is `0 < currentHP` (field
`+0x1b8`, i.e. ATR_HITPOINTS > 0 — "alive"). There is **no** state-flag gate
inside it for combat, weapon-drawn, swimming/diving, or spell-casting; the
per-channel guards are purely `regeneratehp > 0` and `attribute < attributeMax`.
(A separate burn-timer block at `+0x7dc` handles fire damage and is unrelated.)

The caller is the oCNpc per-frame tick at `0x00741fd0`'s only xref,
`0x0073e4d1`, inside the unnamed oCNpc tick function whose prologue is at
`0x0073e480`. Disassembly shows Regenerate is invoked **unconditionally**:

- `0x0073e49d..0x0073e4ca` — a *conditional* block runs `UpdateNextVoice`
  (`0x0073e3c0`) only when the voice counter `[esi+0x748] > 0`.
- `0x0073e4cf` — `mov ecx,esi`
- `0x0073e4d1` — `call 0x00741fd0` (Regenerate), with no preceding branch that
  could skip it for combat / weapon / swim / cast state. Regenerate sits near
  the very top of the tick, **before** the AI/spell event dispatch (the virtual
  `call [eax+0x104]` at `0x0073e506` and everything after).

So in the original, an NPC/player regenerates every tick while alive,
*including while channeling/casting a spell*.

## OG file:line

`/Users/admin/Downloads/opengothic/game/world/objects/npc.cpp:2537` (the
`if(tickCast(dt)) return;` early-return) and `:2540-2552` (the regen block that
sits *after* it).

```
2535  nextAiAction(aiQueueOverlay,dt);
2537  if(tickCast(dt))
2538    return;                     // <-- bails out of tick() during EVERY cast phase
2540  if(!isDead()) {
2547    if(!isImmortal())
2548      tickRegen(... ATR_HITPOINTS ...);
2550    tickRegen(... ATR_MANA ...);
2552  }
```

## Divergence

`Npc::tickCast` (`npc.cpp:4250`) returns `true` for the whole cast pipeline —
cast anim (`CS_Cast_*`), invest/channel (`CS_Invest_*`), emit (`CS_Emit_*`) and
finalize — which triggers the `if(tickCast(dt)) return;` early-return at line
2537, located *before* the regen block. Consequently OpenGothic **pauses HP and
mana regeneration for the entire duration a spell is being cast/channeled**.
The original ZenGin tick calls `oCNpc::Regenerate` unconditionally above the AI
dispatch, so regen keeps running during casting. Most visible for a caster with
`ATR_REGENERATEMANA > 0` holding a long invest spell: original trickles mana
back each tick, OpenGothic freezes it until the cast ends.

## Proposed patch

Move the regen block so it runs before the cast early-return (mirroring the
original ordering, where Regenerate is dispatched near the top of the tick,
ahead of spell/AI event processing). The regen block only reads/writes
attributes and needs nothing tickCast sets up, so the move is side-effect-free.

OLD (`npc.cpp` ~2535-2552):
```cpp
  nextAiAction(aiQueueOverlay,dt);

  if(tickCast(dt))
    return;

  if(!isDead()) {
    // NOTE: in original-game oCNpc::Regenerate @0x00741fd0 the per-tick HP regen ...
    if(!isImmortal())
      tickRegen(hnpc->attribute[ATR_HITPOINTS],hnpc->attribute[ATR_HITPOINTSMAX],
                hnpc->attribute[ATR_REGENERATEHP],dt);
    tickRegen(hnpc->attribute[ATR_MANA],hnpc->attribute[ATR_MANAMAX],
              hnpc->attribute[ATR_REGENERATEMANA],dt);
    }
```

NEW:
```cpp
  nextAiAction(aiQueueOverlay,dt);

  // NOTE: in original-game the per-frame oCNpc tick (caller @0x0073e4d1) invokes
  // oCNpc::Regenerate @0x00741fd0 UNCONDITIONALLY, near the top of the tick and
  // BEFORE spell/AI event dispatch -- regen keeps running while a spell is being
  // cast/channeled. OpenGothic's `if(tickCast(dt)) return;` early-return sat in
  // front of the regen block, so HP/mana regen was wrongly frozen for the whole
  // cast pipeline. Run regen before the cast bail-out to match.
  if(!isDead()) {
    // NOTE: in original-game oCNpc::Regenerate @0x00741fd0 the per-tick HP regen ...
    if(!isImmortal())
      tickRegen(hnpc->attribute[ATR_HITPOINTS],hnpc->attribute[ATR_HITPOINTSMAX],
                hnpc->attribute[ATR_REGENERATEHP],dt);
    tickRegen(hnpc->attribute[ATR_MANA],hnpc->attribute[ATR_MANAMAX],
              hnpc->attribute[ATR_REGENERATEMANA],dt);
    }

  if(tickCast(dt))
    return;
```

(The existing immortal-HP NOTE on the regen block is preserved; the rate-
reciprocal / mana-gated-on-regeneratehp family is untouched.)

Symbols verified present: `Npc::tickCast`, `Npc::isDead`, `Npc::isImmortal`,
`Npc::tickRegen`, `ATR_REGENERATEMANA` (npc.cpp).
