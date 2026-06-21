# Regeneration: rate vs interval (possible reciprocal) — DEFER (needs data/runtime)

**Confidence: Medium (mechanism verified; impact unconfirmed)**

## Original
`oCNpc::Regenerate` (Gothic2.exe `0x00741fd0`). HP and mana each have a countdown
accumulator (`+0x7c4` HP, `+0x7c8` mana) decremented by the frame dt. When the
accumulator drops below 0 (and `attr < max` and the regen field `> 0`), the engine
adds **+1** point and **resets the accumulator to `regenField * 1000` ms**:

- `+0x1d0` = `attr[6]` = `ATR_REGENERATEHP`  → +1 HP every `ATR_REGENERATEHP` seconds.
- `+0x1d4` = `attr[7]` = `ATR_REGENERATEMANA` → +1 mana every `ATR_REGENERATEMANA` seconds.

(Offsets confirmed: `+0x1b8`=ATR_HITPOINTS, `+0x1bc`=…MAX, `+0x1c0`=MANA, `+0x1c4`=MANAMAX,
so `+0x1d0`/`+0x1d4` are attribute indices 6/7.) So the regen field is an **interval in
seconds per point**; effective rate = `1 / regenField` points per second.

## OpenGothic
`Npc::tickRegen` (npc.cpp:2313), called with `chg = ATR_REGENERATEHP` /
`ATR_REGENERATEMANA` (npc.cpp:2389-2392), computes `val = (time*chg)/1000`, i.e. it adds
**`chg` points per second**. So OpenGothic treats the same field as a **rate (points per
second)** — the **reciprocal** of the original's interval semantics.

## Divergence (if the field is nonzero)
For `ATR_REGENERATEHP = N`: original heals `1/N` HP/s; OpenGothic heals `N` HP/s. For
`N>1` these differ by orders of magnitude.

## Why NOT applied
1. **Reciprocal risk** — flipping the interpretation blind would massively change regen if
   the value semantics are not exactly as decompiled; getting it wrong breaks all regen.
2. **Likely near-dead in vanilla** — G2 NPCs/monsters generally do not carry a nonzero
   HP-regen attribute (in-combat HP regen would be very visible and isn't observed), so the
   divergence may rarely or never manifest with retail scripts; `chg==0` makes both engines
   no-op.

Resolving needs the actual `ATR_REGENERATEHP/MANA` values used by retail and mod scripts
(and an in-game check) to confirm which interpretation the data assumes. Documented here
as a suspected divergence rather than a speculative reciprocal patch.
