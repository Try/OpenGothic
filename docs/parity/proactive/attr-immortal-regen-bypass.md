# Per-tick HP regeneration bypasses the IMMORTAL attribute-clamp guard

**Confidence:** Medium

## Original function + address
`oCNpc::Regenerate` (Gothic2.exe `0x00741fd0`) drives the per-frame natural
regeneration. For the HP slot it does NOT touch the attribute array directly:
once its HP countdown timer crosses zero, and only while `HITPOINTS > 0` and
`HITPOINTS < HITPOINTSMAX`, it raises HP by calling
`oCNpc::ChangeAttribute(this, attribIndex=0 /*HITPOINTS*/, +1)`
(`0x0072ff60`) and then reloads the timer from `REGENERATEHP`. Mana regen
likewise routes through `ChangeAttribute(MANA, +1)`.

Because the raise goes through `ChangeAttribute`, it inherits that function's
guard clause: the IMMORTAL flag (oCNpc+0x1b4 bit1) rejects EVERY HITPOINTS
change — positive or negative — except the `-999` kill sentinel. So in the
original an immortal NPC never regenerates HP; its HP is frozen wherever it
currently sits. The same guard does NOT block the MANA index, so mana regen is
unaffected for immortal NPCs.

## OpenGothic file:line
`game/world/objects/npc.cpp:2499-2504` (the `tick()` regen call site) feeding
`Npc::tickRegen` at `game/world/objects/npc.cpp:2424-2440`.

`tickRegen` writes the attribute through a reference
(`v = nextV;`, line 2436) and never consults `isImmortal()`. The call site
invokes it unconditionally inside `if(!isDead())`. This is a different code path
from `Npc::changeAttribute` (line 1251), so the existing immortal fix at
`npc.cpp:1268` — which only guards the `changeAttribute` entry — does not cover
regeneration. The note attached to that fix explicitly claims it stops "natural
regen" from raising an immortal NPC's HP, but natural regen never calls
`changeAttribute`, so the leak persists.

## Divergence
An immortal NPC left below max HP — e.g. damaged while mortal and then flagged
via `Npc_SetImmortal`, or an instance scripted with `HITPOINTS < HITPOINTSMAX`
— is frozen at that HP in the original (every `ChangeAttribute(HITPOINTS,+1)`
regen tick is rejected by the immortal guard). In OpenGothic `tickRegen` writes
HP directly and clamps to `[0, HITPOINTSMAX]`, so the immortal NPC regenerates
all the way back to full HP. (A negative `REGENERATEHP` would, symmetrically,
drain an immortal NPC's HP in OG, which the original also forbids.) Mana regen
matches the original on both engines.

## Proposed patch
Gate only the HP regen tick on the immortal flag, matching the original's
`ChangeAttribute(HITPOINTS,…)` routing; leave mana regen alone (the original's
immortal guard is HP-index only).

OLD (`game/world/objects/npc.cpp:2499`):
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
    // NOTE: in original-game oCNpc::Regenerate @0x00741fd0 the per-tick HP regeneration is
    // applied via oCNpc::ChangeAttribute(HITPOINTS,+1) @0x0072ff60, whose IMMORTAL guard
    // (oCNpc+0x1b4 bit1) rejects every HP change except the -999 kill sentinel. So an immortal
    // NPC never regenerates HP. MANA regen routes through ChangeAttribute(MANA,+1), which that
    // guard does NOT block. OpenGothic's tickRegen writes the attribute directly, bypassing the
    // guard, so an immortal NPC left below max (e.g. damaged then Npc_SetImmortal) regenerates
    // back to full -- the changeAttribute() immortal fix above never covers the regen path.
    if(!isImmortal())
      tickRegen(hnpc->attribute[ATR_HITPOINTS],hnpc->attribute[ATR_HITPOINTSMAX],
                hnpc->attribute[ATR_REGENERATEHP],dt);
    tickRegen(hnpc->attribute[ATR_MANA],hnpc->attribute[ATR_MANAMAX],
              hnpc->attribute[ATR_REGENERATEMANA],dt);
    }
```

Verified OG symbols: `Npc::isImmortal()` (`npc.cpp:4395`, returns
`hnpc->flags & zenkit::NpcFlag::IMMORTAL`), `Npc::tickRegen` (`npc.cpp:2424`),
`ATR_HITPOINTS/ATR_HITPOINTSMAX/ATR_REGENERATEHP/ATR_MANA/ATR_MANAMAX/ATR_REGENERATEMANA`
(used at the existing call site).
