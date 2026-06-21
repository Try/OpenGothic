# Immortal NPCs can be healed/regenerated via ChangeAttribute (positive HP leaks past the immortal guard)

Confidence: Medium-High

## Original function + address

`oCNpc::ChangeAttribute` at 0x0072ff60 (warm Ghidra decompiler over Gothic2.exe).

The function body is gated behind a single compound `if`. Reduced to three clauses
(body runs iff A && B && C):

- A (god-mode): `(value >= 0) || (this != player) || (godmode == 0)`
  -> skip only when value is negative AND this is the player AND god-mode is on.
  This clause is sign-dependent: positive values always pass it.
- B: `value != 0`.
- C (immortal): `(attrIndex != 0) || ((flags & 2) == 0) || (value == -999)`
  -> skip only when attrIndex == 0 (HITPOINTS) AND the IMMORTAL flag (mask 2) is set
  AND value is not the -999 sentinel.

The key observation: clause C, the IMMORTAL guard, is **sign-independent**. It does NOT
test the sign of `value`. So for an immortal NPC, ANY HITPOINTS change -- positive heals
and regeneration as well as negative damage -- is dropped, except the -999 kill sentinel.
Only the god-mode clause A is restricted to negative values.

This is confirmed by callers that pass positive HP deltas through this same function:
`oCNpc::Regenerate` (0x00742036) calls it with (attrIndex 0 / HP, +1) and (attrIndex 2 /
MANA, +1) every regen tick, and `oCNpc::AddItemEffects` (0x00732132) routes
potion/food heal effects through it. In the original, an immortal NPC therefore neither
regenerates HP nor is healed by these effects.

IMMORTAL = 1U << 1U (mask 2) is confirmed in
`lib/ZenKit/include/zenkit/addon/daedalus.hh` (NpcFlag::IMMORTAL).

## OpenGothic file:line

`game/world/objects/npc.cpp:1244` `Npc::changeAttribute`, guard block lines 1248-1259.

The IMMORTAL check is nested inside `if(val<0 && a==ATR_HITPOINTS)`:

```
if(val<0 && a==ATR_HITPOINTS) {
  ...
  if(isImmortal() && val!=-999)
    return;
  }
```

Because the immortal `return` only lives in the `val<0` branch, positive HP changes for an
immortal NPC skip the guard entirely and apply.

## Divergence

In the original game the IMMORTAL flag freezes an NPC's HITPOINTS against every change
(positive or negative) except the -999 sentinel: an immortal NPC does not regenerate HP
and cannot be healed by a healing potion / food effect via Npc_ChangeAttribute. In
OpenGothic the immortal guard is gated on `val<0`, so an immortal NPC's HITPOINTS still
climb from natural regeneration (Npc::tick regen path) and from positive
`Npc_ChangeAttribute(npc, ATR_HITPOINTS, +N)` / healing-potion effects. Result: an immortal
NPC that has been damaged down (e.g. an essential plot NPC clamped to 1 HP, or one whose HP
was scripted low) will heal back up in OpenGothic where the original keeps it pinned. The
behavior also differs for any script that uses a positive Npc_ChangeAttribute on an
immortal NPC expecting it to be a no-op.

Note: the clamp-to-max and floor-at-0 are already verified faithful, and the -999
negative-kill bypass is already fixed (docs/parity/proactive/attr-immortal-kill-999.md);
this finding is the complementary positive-direction leak of the same immortal guard.

## Proposed patch

File: `game/world/objects/npc.cpp`

Move the IMMORTAL guard out of the `val<0` branch so it covers positive HP changes too,
keeping the -999 escape and the existing godmode/cutscene guards (which are correctly
negative-only) in place.

OLD:
```
  if(val<0 && a==ATR_HITPOINTS) {
    if(isPlayer() && Gothic::inst().isGodMode())
      return;
    if(isPlayer() && owner.currentCs()!=nullptr)
      return;
    // NOTE: in original-game oCNpc::ChangeAttribute (Gothic2.exe 0x0072ff60) the IMMORTAL
    // flag is bypassed for the sentinel val==-999, so scripts/cutscenes can force-kill an
    // immortal NPC via Npc_ChangeAttribute(self,ATR_HITPOINTS,-999). Blocking it silently
    // dropped those scripted deaths.
    if(isImmortal() && val!=-999)
      return;
    }
```

NEW:
```
  if(val<0 && a==ATR_HITPOINTS) {
    if(isPlayer() && Gothic::inst().isGodMode())
      return;
    if(isPlayer() && owner.currentCs()!=nullptr)
      return;
    }

  // NOTE: in original-game oCNpc::ChangeAttribute (Gothic2.exe 0x0072ff60) the IMMORTAL
  // guard on HITPOINTS is sign-independent: it drops ALL HP changes (positive heals/regen
  // as well as damage) for an immortal NPC, with the lone exception of the -999 kill
  // sentinel. Only the god-mode/cutscene guards above are negative-only. Nesting the
  // immortal check under val<0 let positive heals (Regenerate, healing-potion AddItemEffects)
  // leak through and raise an immortal NPC's HP.
  if(a==ATR_HITPOINTS && isImmortal() && val!=-999)
    return;
```

Build-verifiable: `isImmortal()`, `Attribute ATR_HITPOINTS`, and the surrounding guards are
all existing symbols in this function (grep-verified at npc.cpp:1244-1259, 4206).
