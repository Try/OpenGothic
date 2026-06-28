# mob2 — decoration-mob guild-ownership theft gate is dropped (`Npc_IsDetectedMobOwnedByGuild` stubbed, `owner_guild` never read)

**Confidence:** High (divergence confirmed in both binary and source; fix follows the existing `owner` field pattern verbatim).

## Original fn + address (prose)

A decoration-mob (oCMOB / oCMobInter: bed, bench, chair, anvil, …) carries **two** ownership
fields in the original engine, both set by `oCMOB::SetOwner` (Gothic2.exe `0x0071bf80`):

- the owner-NPC, stored at `+0x174` as the parser symbol index of the owner instance string, and
- the owner-**guild**, stored at `+0x178` as the *integer value* of the guild-constant symbol named
  by the `ownerGuild` string. `SetOwner` uppercases that string, looks it up in the parser symbol
  table (`zCParser::GetSymbol`, FUN_007938d0) and reads its constant value (FUN_007a1fe0); when the
  string is empty or unresolved it stores `-1`.

`oCMOB::IsOwnedByGuild(int guild)` (Gothic2.exe `0x0071c190`) returns `0` when `+0x178 < 0`,
otherwise `(+0x178 == guild)`. The Daedalus external `Npc_IsDetectedMobOwnedByGuild(self, guild)`
(handler FUN_006ed750 @ `0x006ed750`) takes the user's detected mob — `oCNpc::GetInteractMob`
with a fallback to the move-collision `RbtObstacleVob` cast to `oCMobInter` — and calls
`IsOwnedByGuild(guild)` on it. This is the sibling of `Npc_IsDetectedMobOwnedByNpc` (`0x006ed540` →
`oCMOB::IsOwnedByNpc` `0x0071c1b0`) and is what scripted use-mob assessment (B_AssessUseMob and
friends) relies on to make a guild react to the hero using a bed/chair/anvil that belongs to that
guild rather than to one named NPC.

## OG file:line

- `game/world/objects/interactive.cpp:34` — the constructor reads `owner = vob.owner;` only and
  never reads `vob.owner_guild` (zenkit *does* parse it: `VMovableObject::owner_guild`,
  `lib/ZenKit/include/zenkit/vobs/MovableObject.hh:83`). There is no `ownerGuild` member on
  `Interactive`, and it is absent from `load`/`save` (`interactive.cpp:111-117`, `150-156`).
- `game/game/gamescript.cpp:3078-3095` — `npc_isdetectedmobownedbyguild` is an explicit stub:
  it logs `"not implemented call [npc_isdetectedmobownedbyguild]"` once and always returns `false`.

## Divergence

The guild-ownership branch of the mob theft/trespass gate is non-functional in OpenGothic. The
NPC-owner branch (`Npc_IsDetectedMobOwnedByNpc`) is implemented and correct, but its guild twin
always returns `false`, and the underlying `owner_guild` data is silently discarded at load time.
Result: a guild-owned decoration mob (a bed/bench in a faction building whose `ownerGuild` is set
rather than `owner`) never triggers the guild's "that's ours" reaction — the original engine would
have an owning-guild member assess theft/usage, OpenGothic treats it as un-owned.

## Proposed patch (DO NOT apply here — doc only)

Mirror the existing `owner` field exactly, plus a `Serialize` version bump.

`game/world/objects/interactive.h` — add next to `owner` (~line 159):
```
    std::string         owner;
+   std::string         ownerGuild;
```
and an accessor next to `ownerName()` (~line 45):
```
    std::string_view    ownerName() const;
+   std::string_view    ownerGuildName() const;
```

`game/world/objects/interactive.cpp` constructor (after line 34):
```
   owner         = vob.owner;
+  ownerGuild    = vob.owner_guild;
```
(the existing `for(auto& i:owner) i = toupper(i);` loop at lines 52-53 should be extended to
uppercase `ownerGuild` as well, matching `oCMOB::SetOwner`'s upper-casing before symbol lookup.)

`Interactive::load`/`save` — append `ownerGuild`, version-guarded (bump
`Serialize::Version::Current` 57→58 in `game/game/serialize.h:36`):
```
  fin.read(bbox[0],bbox[1],owner);
+ if(fin.version()>=58) fin.read(ownerGuild);
...
  fout.write(bbox[0],bbox[1],owner);
+ fout.write(ownerGuild);
```

Accessor + external (`game/world/objects/interactive.cpp` near line 453, and
`game/game/gamescript.cpp:3078`):
```
std::string_view Interactive::ownerGuildName() const { return ownerGuild; }
```
```
bool GameScript::npc_isdetectedmobownedbyguild(std::shared_ptr<zenkit::INpc> npcRef, int guild) {
  auto npc = findNpc(npcRef);
  if(npc==nullptr || npc->detectedMob()==nullptr)
    return false;
  // NOTE: in original-game Npc_IsDetectedMobOwnedByGuild (handler @0x006ed750) calls
  // oCMOB::IsOwnedByGuild @0x0071c190, which resolves the mob's ownerGuild string (uppercased,
  // via the parser symbol's int value in oCMOB::SetOwner @0x0071bf80) and returns
  // (ownerGuild>=0 && ownerGuild==guild). OpenGothic stubbed this to always-false and dropped
  // vob.owner_guild entirely, so a guild-owned decoration mob never triggered the guild reaction.
  auto ow = npc->detectedMob()->ownerGuildName();
  if(ow.empty())
    return false;
  auto* sym = vm.find_symbol_by_name(std::string(ow));
  if(sym==nullptr)
    return false;
  return sym->get_int()==guild;
  }
```

This keeps plain (un-owned, or NPC-owned-only) mobs unaffected: an empty `ownerGuild` returns
`false`, exactly as `IsOwnedByGuild` returns `0` for the `-1` sentinel.
