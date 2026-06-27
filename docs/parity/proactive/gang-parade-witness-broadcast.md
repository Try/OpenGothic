# Parried/blocked hit does not run AssessDamage or recruit witnesses (ally-assist gap)

**Confidence:** High

## Original function + address
`oCNpc::EV_Parade` (Gothic2.exe `0x007522d0`) is the defender's parade message
handler. When the parade animation is actually started (the successful-block
branch), the very last thing it does is call `oCNpc::AssessDamage_S(defender,
attacker, value=0)`.

`oCNpc::AssessDamage_S` (Gothic2.exe `0x0075c280`) does two things, in order, and
the second is **unconditional** (it is reached on a fall-through `goto` even when
the defender has no ASSESSDAMAGE perception registered):
1. Runs the defender's own `PERC_ASSESSDAMAGE` (perc 8) state via `StartAIState`
   (the "I was attacked, react / fight back" reaction, i.e. script `B_AssessDamage`).
2. Calls `oCNpc::CreatePassivePerception(perc=9 = PERC_ASSESSOTHERSDAMAGE,
   OTHER=attacker, VICTIM=defender)` — the witness broadcast that alerts/recruits
   nearby NPCs (`B_AssessOthersDamage`, the gang-aggro / guild-mate-help path).

So in the original, **successfully parrying a blow still makes the defender
assess its attacker and still calls its comrades** — the net damage value passed
to `AssessDamage_S` is literally `0`, so there is no damage-value dependency.
`CreatePassivePerception` itself gates each receiver only on: is an `oCNpc`, has
that perception registered (funcId >= 0), is not the sender, is alive (HP > 0),
and is not the player — there is no guild/senses gate at the broadcast level.

## OpenGothic file:line
`game/world/objects/npc.cpp:2086-2091` (`Npc::takeDamage(Npc& other, const Bullet* b)`)

```cpp
  if(!(isBlock || isJumpb) || b!=nullptr || flyAtk) {
    takeDamage(other,b,COLL_DOEVERYTHING,0,false);
    } else {
    if(invent.activeWeapon()!=nullptr)
      visual.emitBlockEffect(*this,other);
    }
```

## Divergence
The self `PERC_ASSESSDAMAGE` (npc.cpp:2145) and the witness `PERC_ASSESSOTHERSDAMAGE`
broadcast (npc.cpp:2193) live only inside the full-damage overload
`takeDamage(other,b,COLL_DOEVERYTHING,...)`. On a successful **block/parade**
OpenGothic takes the `else` branch and runs *only* `visual.emitBlockEffect`. The
sender still emits `PERC_ASSESSFIGHTSOUND` at line 2084, but neither the defender's
own ASSESSDAMAGE reaction nor the ASSESSOTHERSDAMAGE ally-recruit broadcast fires.

Consequence: an NPC (e.g. a guard) who perfectly parries the player's swings never
runs `B_AssessDamage` and never alerts its guild-mates, so a fight that opens with
a blocked hit fails to turn the parrying NPC hostile or to pull in nearby allies —
unlike vanilla, where the parry path routes through `AssessDamage_S` with value 0.
This is independent of the already-noted `value>0` nesting at line 2192 (that note
only covers the *damaging* path; the *parried* path is a separate omission).

## Proposed patch
Gate the addition on `isBlock` only (a pure `isJumpb` dodge is not a parade in the
original — no EV_Parade is sent, the strike simply whiffs):

OLD (`game/world/objects/npc.cpp`, block branch ~2088):
```cpp
    } else {
    if(invent.activeWeapon()!=nullptr)
      visual.emitBlockEffect(*this,other);
    }
```
NEW:
```cpp
    } else {
    if(invent.activeWeapon()!=nullptr)
      visual.emitBlockEffect(*this,other);
    // NOTE: in original-game oCNpc::EV_Parade (Gothic2.exe 0x007522d0) a started parade ends with
    // oCNpc::AssessDamage_S(defender, attacker, value=0) (Gothic2.exe 0x0075c280), which runs the
    // defender's own PERC_ASSESSDAMAGE and unconditionally broadcasts PERC_ASSESSOTHERSDAMAGE
    // (CreatePassivePerception perc 9, OTHER=attacker, VICTIM=defender) to nearby witnesses. So a
    // blocked blow still makes the defender react and still recruits its guild-mates. OpenGothic ran
    // neither on a pure block, so a perfectly-parried attacker stayed un-assessed and no allies were
    // alerted. Mirror lines 2145 / 2193 for the parade path (not the isJumpb dodge).
    if(isBlock) {
      perceptionProcess(other,this,0,PERC_ASSESSDAMAGE);
      owner.sendPassivePerc(*this,other,*this,PERC_ASSESSOTHERSDAMAGE);
      }
    }
```

Grep-verified OG symbols: `Npc::perceptionProcess(Npc&,Npc*,float,PercType)`
(npc.cpp:4408, called identically at npc.cpp:2145), `World::sendPassivePerc(Npc&,
Npc&,Npc&,int)` (world.cpp:710, called identically at npc.cpp:2193), `isBlock`
(local bool, npc.cpp:2072), `PERC_ASSESSDAMAGE`/`PERC_ASSESSOTHERSDAMAGE`
(constants.h:417/418). The arg order (self=defender, other=attacker,
victim=defender) reproduces the existing damage-path broadcast at npc.cpp:2193 and
matches `AssessDamage_S`'s OTHER=attacker / VICTIM=this instancing.
