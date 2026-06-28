# Spawn-time clamp of `C_NPC.weapon` to [0,7] (else 0)

**Confidence:** Medium

## Original fn + address
`oCNpc::InitByScript` @ `0x0072ee70`.

Right after `zCParser::CreateInstance` populates the script class, the routine
runs the same family of spawn-time fixups that already gave us `body_mass=10`
(`this+0x25c`) and `damage_type=2` (`this+0x22c`). Inside the
"freshly-spawned, not loaded from save" branch (`param_2 == 0`) it does:

```
if (this+0x250 < 0 || 7 < this+0x250)  this+0x250 = 0;   // clamp
if (this+0x250 == 1)                   SetToFistMode();   // weapon==1 -> fists
else                                   <draw right-hand slot weapon>
```

Offset mapping (C_NPC data block starts at `this+0x120`, `zSTRING=0x14`):
`id`(0) + `name[5]`(100) + `slot`(20) + `effect`(20) + `type`(4) + `flags`(4) +
`attribute[8]`(32) + `hitchance[5]`(20) + `protection[8]`(32) + `damage[8]`(32)
= 264, so `damage_type`@268 → `this+0x22c` (matches established), `guild`@272 →
`this+0x230` (matches), `level`@276, `mission[5]`@280..300, `fight_tactic`@300 →
`this+0x24c`, **`weapon`@304 → `this+0x250`**, `voice`@308, `voice_pitch`@312,
`body_mass`@316 → `this+0x25c` (matches). Therefore the field clamped at
`this+0x250` is `C_NPC.weapon` (the ready/drawn-weapon mode: 0=none, 1=fist,
3=1h, 4=2h, 5=bow, 6=crossbow, 7=magic — same encoding OpenGothic writes in
`Npc::drawWeapon*`).

## OG file:line
`game/world/objects/npc.cpp:180-206` (`Npc::Npc` ctor). The ctor calls
`initializeInstanceNpc`, then applies the `damage_type` and `body_mass`
defaults, but never validates `hnpc->weapon`. `grep` confirms the field exists
(`serialize.cpp:300/324`, `npc.cpp:3604/3614/...`) and the ctor never touches
it.

## Divergence
The engine guarantees a spawned NPC's `weapon` mode is in `[0,7]`, resetting any
out-of-range instance value to 0. OpenGothic leaves whatever the Daedalus
instance set, so a script that assigns `C_NPC.weapon` an out-of-range value
(or any nonzero value that OG's runtime weapon-state machine never expects at
spawn) starts the NPC in an inconsistent ready-weapon state. Only the clamp is
ctor-safe; the `weapon==1 -> SetToFistMode()` / draw-equipped-weapon behavior is
NOT replicated here because `Npc::setToFistMode()` touches `visual`, which is
not yet attached during construction (see DEFERRED note below).

## Proposed patch
```cpp
// OLD (game/world/objects/npc.cpp, after the body_mass default)
  if(hnpc->body_mass==0)
    hnpc->body_mass = 10;
  setTrueGuild(hnpc->guild); // ...

// NEW
  if(hnpc->body_mass==0)
    hnpc->body_mass = 10;
  // NOTE: in original-game oCNpc::InitByScript @0x0072ee70, right after
  // CreateInstance the engine clamps C_NPC.weapon (engine offset 0x250) to the
  // [0,7] ready-weapon range, resetting any out-of-range instance value to 0,
  // in the same spawn-fixup block as the body_mass/damage_type defaults above.
  if(hnpc->weapon<0 || hnpc->weapon>7)
    hnpc->weapon = 0;
  setTrueGuild(hnpc->guild); // ...
```

DEFERRED (related, not patched): the engine's `weapon==1 -> SetToFistMode()` and
"draw equipped right-hand weapon" spawn behavior. Replicating it in the ctor is
not surgical because `Npc::setToFistMode()` / weapon-draw paths dereference
`visual`, which is assigned after construction; doing it here risks a
null/half-built visual. Belongs in post-spawn init, not the ctor.
