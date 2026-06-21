# Immortal NPCs cannot be killed via the -999 sentinel

Confidence: High

## Original function

`oCNpc::ChangeAttribute` at 0x0072ff60 (warm Ghidra). The function gates the whole
attribute write behind two guards:

1. God-mode guard: if the value is negative AND the npc is the player AND god-mode is
   active, the change is dropped (applies to any attribute index).
2. Immortal guard: for the HITPOINTS attribute (index 0) only, if the npc has the
   IMMORTAL flag (NpcFlag bit 1, mask 2), the change is dropped — UNLESS the passed
   value is exactly -999. The decompiled branch reads
   `(attrIndex != 0) || ((flags & 2) == 0) || (value == -999)`, i.e. -999 is an
   explicit escape hatch that lets a script kill an otherwise-immortal NPC.

After the guards: add value, clamp to >= 0, and clamp HITPOINTS / MANA to their
respective MAX. (The -999 magnitude overshoots, then the >=0 clamp pins HP to 0, so the
death path fires.)

IMMORTAL = 1U << 1U is confirmed in
`lib/ZenKit/include/zenkit/addon/daedalus.hh:166`, matching the `flags & 2` test.

## OpenGothic

`game/world/objects/npc.cpp:1236` `Npc::changeAttribute`, guard block lines 1240-1247:

```
if(val<0 && a==ATR_HITPOINTS) {
  ...
  if(isImmortal())
    return;
  }
```

## Divergence

OpenGothic drops every negative HITPOINTS change for an immortal NPC with no exception.
The original allows the special value -999 to bypass the immortal flag and apply the
damage (and thus trigger death). Daedalus scripts use
`Npc_ChangeAttribute(npc, ATR_HITPOINTS, -999)` as the canonical way to kill an
immortal NPC in cutscenes / scripted deaths. In OpenGothic such a call is silently
ignored, leaving the NPC alive at full HP — a scripted death that simply does not happen.

## Proposed patch

File: `game/world/objects/npc.cpp`

OLD:
```
  if(val<0 && a==ATR_HITPOINTS) {
    if(isPlayer() && Gothic::inst().isGodMode())
      return;
    if(isPlayer() && owner.currentCs()!=nullptr)
      return;
    if(isImmortal())
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
    // NOTE: in original-game (oCNpc::ChangeAttribute @0x0072ff60) the IMMORTAL flag
    // blocks negative HITPOINTS changes UNLESS the value is exactly -999, the sentinel
    // scripts use (Npc_ChangeAttribute(npc,ATR_HITPOINTS,-999)) to kill immortal NPCs.
    if(isImmortal() && val!=-999)
      return;
    }
```
