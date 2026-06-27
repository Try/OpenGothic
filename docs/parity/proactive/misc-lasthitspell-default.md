# Npc_GetLastHitSpellID / Npc_GetLastHitSpellCat return 0 instead of -1 for an NPC never hit by a spell

**Confidence:** High (for the never-hit default divergence; the post-hit category derivation is a separate, lower-confidence concern noted at the end).

## Original function + address (prose)

- `Npc_GetLastHitSpellID` handler is `FUN_006e5660`. It fetches the `self` Npc and, when non-null,
  returns the raw oCNpc field at offset `+0x920` (the stored "last hit spell id").
- `Npc_GetLastHitSpellCat` handler is `FUN_006e5770`. It likewise returns the raw oCNpc field at
  offset `+0x924` (a *separately stored* "last hit spell category"). It does **not** look the
  category up from the id; it is a distinct field.
- The oCNpc constructor `oCNpc::oCNpc` @ `0x0072d950` initializes **both** fields to `-1`
  (it writes `0xffffffff` to `this+0x920` and `this+0x924`). So for any normally spawned NPC that
  has not yet been hit by a spell, both externals return `-1`.
  (Supporting observation: scanning the binary for writes to `+0x924` — via `wde find 0x924` and the
  offsets index — turns up only the constructor, the save/load path, and the getter itself; no
  spell/damage code was found writing the category field. This suggests the category is effectively
  always `-1` in the original, but that stronger claim is not load-bearing for the fix below.)

## OpenGothic file:line

- `game/world/objects/npc.h:590` — `int32_t lastHitSpell = 0;`  (default is **0**, not -1)
- `game/world/objects/npc.h:401` — `int32_t lastHitSpellId() const { return lastHitSpell; }`
- `game/game/gamescript.cpp:2925-2931` — `npc_getlasthitspellid` returns `npc->lastHitSpellId()` directly.
- `game/game/gamescript.cpp:2933-2941` — `npc_getlasthitspellcat` returns `spellDesc(lastHitSpellId()).spell_type`.

## Divergence

For an NPC that has never been hit by a spell:

- `Npc_GetLastHitSpellID`: original returns **-1**; OpenGothic returns **0** (the `lastHitSpell`
  default). Spell index 0 is a *valid* `C_Spell` instance, so scripts that branch on
  `Npc_GetLastHitSpellID(self) == SPL_xxx` (e.g. magic-reaction perceptions) can spuriously match
  spell 0 before any spell has actually hit, whereas the original's -1 never matches a real spell id.
- `Npc_GetLastHitSpellCat`: original returns the stored category field (default **-1**); OpenGothic
  derives `spellDesc(0).spell_type` from the default id, returning a real spell type instead of -1.

OpenGothic models the category as a *derivation from the id* rather than a stored field; the
constructor-default mismatch (0 vs -1) is the concrete, airtight part of the divergence.

## Proposed patch

The id field is set to a real `splId` on every spell hit (`npc.cpp:2132` in `takeDamage`, and
`npc.cpp:3385` for instant spells), so changing only the default brings the never-hit case into line
with the original without affecting post-hit behavior. The category external must also be guarded,
because once the default is -1, `spellDesc(uint16_t(-1))` would index `spellFxInstanceNames` out of
range.

OLD (`game/world/objects/npc.h:590`):
```cpp
    int32_t                        lastHitSpell     = 0;
```
NEW:
```cpp
    // NOTE: in original-game oCNpc::oCNpc @0x0072d950 the last-hit-spell id field (+0x920)
    // is initialized to -1, so Npc_GetLastHitSpellID (FUN_006e5660) returns -1 until a spell hits.
    int32_t                        lastHitSpell     = -1;
```

OLD (`game/game/gamescript.cpp:2933-2941`):
```cpp
int GameScript::npc_getlasthitspellcat(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  if(npc==nullptr)
    return SPELL_GOOD;

  const int id    = npc->lastHitSpellId();
  auto&     spell = spellDesc(id);
  return spell.spell_type;
  }
```
NEW:
```cpp
int GameScript::npc_getlasthitspellcat(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  if(npc==nullptr)
    return SPELL_GOOD;

  const int id = npc->lastHitSpellId();
  // NOTE: in original-game Npc_GetLastHitSpellCat (FUN_006e5770) returns oCNpc+0x924, which the
  // constructor @0x0072d950 initializes to -1; an NPC not yet hit by a spell yields -1 (and
  // guarding here keeps spellDesc() from indexing out of range once the id default is -1).
  if(id<0)
    return -1;
  auto& spell = spellDesc(id);
  return spell.spell_type;
  }
```

### Caveat / partial DEFERRED
The post-hit category value is not fully resolved: the original stores the category in an independent
field (+0x924) for which no spell-hit writer was located, whereas OpenGothic derives it from the id.
If the original's +0x924 is indeed never written outside the constructor, `Npc_GetLastHitSpellCat`
would return -1 even after a spell hit, which the proposed patch does not replicate (it still derives
`spell_type` for id>=0). That post-hit aspect is left **DEFERRED** pending confirmation of a +0x924
setter; the never-hit default fix above is the high-confidence, build-safe portion.
