# Npc_ChangeAttribute: godmode immunity scope (HITPOINTS-only vs all attributes)

**Confidence:** Medium-High (structurally exact read of the original guard; impact limited to the debug/marvin godmode cheat)

## Original function + address

`Npc_ChangeAttribute` external handler `FUN_006e8a40` parses the two integer
parameters (attribute index, delta) and forwards to the method
`oCNpc::ChangeAttribute` at **0x0072ff60** (`P:\dev\g2addon\release\Gothic\_ulf\oNpc.cpp`).

The very first guard of `oCNpc::ChangeAttribute` is a single compound condition. Decoded
into prose, it gates the whole body on:

1. `(delta >= 0) OR (this != oCNpc::player) OR (oCNpc::godmode == 0)` — i.e. when the
   target is the player and godmode is on, **any negative delta is rejected regardless of
   which attribute is being changed**. There is no attribute-index test in this clause.
2. AND `delta != 0`.
3. AND `(atr != 0) OR (immortal flag at +0x1b4 bit1 clear) OR (delta == -999)` — the
   IMMORTAL flag only blocks attribute index 0 (HITPOINTS), with the `-999` kill sentinel
   exempt.

So in the original, the godmode check is attribute-agnostic (blocks negative changes to
HP, MANA, STRENGTH, etc. for the godmode player), while the immortal check is
HITPOINTS-only.

## OpenGothic file:line

`/Users/admin/Downloads/opengothic/game/world/objects/npc.cpp:1244` (`Npc::changeAttribute`),
specifically lines 1248-1253.

## Divergence

OpenGothic scopes the godmode (and the OG-only cutscene) immunity to `a==ATR_HITPOINTS`:

```cpp
if(val<0 && a==ATR_HITPOINTS) {
  if(isPlayer() && Gothic::inst().isGodMode())
    return;
  if(isPlayer() && owner.currentCs()!=nullptr)
    return;
  }
```

The original applies the godmode immunity to *every* attribute, not just HITPOINTS. Under
godmode, the original player cannot lose MANA (spell-cast drain), STRENGTH, DEXTERITY, etc.;
OpenGothic still drains those because the `a==ATR_HITPOINTS` qualifier short-circuits the
godmode test for non-HP attributes. Net effect: a marvin/godmode player in OG can still be
mana-starved or stat-drained, whereas in the original godmode is a full negative-attribute
shield.

(The immortal-only-HP scoping and the `-999` exemption at line 1260-1261 already match the
original and are faithful.)

## Proposed patch

Split the godmode test out of the HITPOINTS qualifier so it covers all attributes, leaving
the OG-only cutscene guard HITPOINTS-scoped (the original has no cutscene clause, so do not
widen that one):

OLD:
```cpp
  if(val<0 && a==ATR_HITPOINTS) {
    if(isPlayer() && Gothic::inst().isGodMode())
      return;
    if(isPlayer() && owner.currentCs()!=nullptr)
      return;
    }
```

NEW:
```cpp
  // NOTE: in original-game oCNpc::ChangeAttribute (Gothic2.exe 0x0072ff60) the godmode guard
  // has no attribute-index test: for the godmode player EVERY negative delta is rejected
  // (HP, MANA, STRENGTH, ...), not just HITPOINTS. Only the IMMORTAL flag is HP-scoped.
  if(val<0 && isPlayer() && Gothic::inst().isGodMode())
    return;
  if(val<0 && a==ATR_HITPOINTS && isPlayer() && owner.currentCs()!=nullptr)
    return;
```

Grep-verified symbols: `Npc::isPlayer()` (npc.cpp:548), `Gothic::inst().isGodMode()`
(npc.cpp:1249), `owner.currentCs()` (npc.cpp:1251), `Attribute::ATR_HITPOINTS=0`
(constants.h:473), `ATR_MANA=2` (constants.h:475).

## Externals checked and found faithful

- **Npc_ChangeAttribute clamps** — the HP/MANA `>` MAX clamps (npc.cpp:1266-1269) and the
  `<0 -> 0` floor match the original (HP clamped to HITPOINTSMAX, MANA clamped to MANAMAX).
- **Npc_PercDisable** (`FUN_006de7d0` -> `oCNpc::DisablePerception`) — thin null-guarded
  forward, matches.
- **Npc_GetDistToWP** (`FUN_006f2c30`) — returns INT_MAX sentinel on missing npc/wp; OG
  matches with `std::numeric_limits<int32_t>::max()`.
- **Npc_SetRefuseTalk / Npc_RefuseTalk** — `max(timeSec*1000,0)` ms duration, faithful.
- **immortal/-999 kill sentinel** in ChangeAttribute — already correct.
