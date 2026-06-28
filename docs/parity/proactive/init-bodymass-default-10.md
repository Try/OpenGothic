# NPC spawn default: `body_mass` falls back to 10 when the instance leaves it 0

**Confidence:** High (offset mapping cross-validated by two independent anchors; see below).

## Original function + address

`oCNpc::InitByScript` @ `0x0072ee70` (Gothic2.exe). This is the engine routine that
runs the Daedalus instance constructor for a C_NPC (`zCParser::CreateInstance`) and then
applies a handful of engine-side spawn-time defaults/clamps before the routine is
initialized.

Immediately after `zCParser::CreateInstance(parser, instanceIndex, this)` returns, and
*before* the `param_2==0` (first-spawn) attitude/weapon block, the engine does an
unconditional fallback:

> if the freshly-constructed C_NPC's `body_mass` field is 0, it is forced to 10.

(In raw form: `if (*(int*)(this+0x25c)==0) *(int*)(this+0x25c)=10;`.) A few lines later
the same function applies the already-known `damage_type==0 -> 2` fallback at `this+0x22c`.

### Offset mapping (why `0x25c` == `body_mass`)
The C_NPC data block starts at `this+0x120` (the function memsets 200 dwords = 800 bytes,
matching `zCParser::CheckClassSize(...,800)`, and writes the instance index to
`*(int*)(this+0x120)`). Walking the Daedalus C_NPC field order (see ZenKit
`INpc` in `lib/ZenKit/include/zenkit/addon/daedalus.hh:185-216`) with int/func = 4 bytes
and engine `zSTRING` = 0x14 bytes:
- `guild` lands at `0x230` — matches the engine line `this[0x766] = guild_byte(this+0x230)`
  used to seed the true-guild. (anchor #1)
- `damage_type` lands at `0x22c` — matches the known `damage_type=0 -> 2` default the
  engine applies there. (anchor #2, already fixed in OG)
- `body_mass` lands at `0x25c` — the field defaulted to 10 here.

Both anchors agree on `zSTRING==0x14`, so the `0x25c == body_mass` identification is solid.

## OG file:line

`/Users/admin/Downloads/opengothic/game/world/objects/npc.cpp:195-198`
(the `Npc::Npc(...)` constructor, right after `initializeInstanceNpc`).

```cpp
  if(hnpc->damage_type==0)
    hnpc->damage_type = 2;
```

`hnpc->body_mass` (ZenKit `INpc::body_mass`, daedalus.hh:203) is value-initialized to 0 by
`std::make_shared<zenkit::INpc>()` and is never re-defaulted. OG only ever touches it in
save/load (`game/game/serialize.cpp:300,324`).

## Divergence

For any C_NPC instance whose Daedalus constructor does not assign `body_mass` (many monster
and ambient-human instances leave it unset), the original engine leaves `body_mass == 10`,
whereas OpenGothic leaves it `0`. This is a script-observable / serialized field
(`self.body_mass` can be read from Daedalus, and the value is round-tripped through saves),
so the spawn-time default diverges by a constant 10-vs-0.

## Proposed patch

OLD (`game/world/objects/npc.cpp`, in `Npc::Npc`):
```cpp
  if(hnpc->damage_type==0)
    hnpc->damage_type = 2;
```

NEW:
```cpp
  if(hnpc->damage_type==0)
    hnpc->damage_type = 2;
  // NOTE: in original-game oCNpc::InitByScript @0x0072ee70, right after CreateInstance,
  // C_NPC.body_mass is forced to 10 when the instance left it at 0 (engine offset 0x25c).
  if(hnpc->body_mass==0)
    hnpc->body_mass = 10;
```

Placed alongside the existing `damage_type` fallback so it runs for every initialized
instance (non `-1`), matching the original's unconditional placement.
