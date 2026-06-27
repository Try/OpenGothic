# Bind Npc_OwnedByGuild — unbound trivial item-ownership predicate

**Confidence:** High

## Original function + address

- Daedalus external string `Npc_OwnedByGuild` at `0x008b556c`-region table entry `0x008b4718`; its
  registered handler is the thunk at `0x006e9ca0` (in `oGameExternal.cpp`, registered by
  `DefineExternals_Ulfi`).
- The handler reads one `int` parameter (the guild id) via `zCParser::GetParameter`, then pops the
  `oCItem` instance, and returns `item->IsOwnedByGuild(guild)` (virtual slot `0x80`).
- `oCItem::IsOwnedByGuild(int)` at `0x007127c0` is literally:
  return non-zero iff `guild > 0` AND the item's owner-guild field (`oCItem+0x204`, i.e.
  `owner_guild`) equals `guild`; otherwise 0.

So the whole external is the one-liner: `Npc_OwnedByGuild(item, guild)` ==
`guild > 0 && item.owner_guild == guild`. Argument order is `(item, guild)`.

This is the guild-symmetric sibling of `Npc_OwnedByNpc` (already bound as `npc_ownedbynpc`), which
compares `item.owner` against an npc instance.

## OpenGothic file:line

- Bind table: `game/game/gamescript.cpp:244` (right after `npc_ownedbynpc`).
- Header decl: `game/game/gamescript.h:367` (right after `npc_ownedbynpc`).
- Impl reference: `game/game/gamescript.cpp:3075` (`GameScript::npc_ownedbynpc`).

## Divergence

`npc_ownedbyguild` is **unbound** (grep count 0 across `game/`; the 5 superficial matches are the
unrelated `npc_isdetectedmobownedbyguild`). When Gothic scripts call `Npc_OwnedByGuild`
(theft / ownership gates), the VM hits a missing external and returns the default 0, i.e. an item is
never treated as owned-by-guild. The original returns a correct true/false.

The building blocks already exist in OG:
- `zenkit::IItem::owner_guild` (`lib/ZenKit/include/zenkit/addon/daedalus.hh:295`), already
  serialized in `game/world/objects/item.cpp:54,124`.
- `Item::handle()` (`game/world/objects/item.h:88`) and `GameScript::findItem(...)`, used identically
  by `npc_ownedbynpc`.

No new subsystem is required.

## Proposed patch

`game/game/gamescript.h` (after line 367):

```cpp
// OLD
    bool npc_ownedbynpc      (std::shared_ptr<zenkit::IItem> itmRef, std::shared_ptr<zenkit::INpc> npcRef);
// NEW
    bool npc_ownedbynpc      (std::shared_ptr<zenkit::IItem> itmRef, std::shared_ptr<zenkit::INpc> npcRef);
    bool npc_ownedbyguild    (std::shared_ptr<zenkit::IItem> itmRef, int guild);
```

`game/game/gamescript.cpp` (after line 244):

```cpp
// OLD
  bindExternal("npc_ownedbynpc",                 &GameScript::npc_ownedbynpc);
// NEW
  bindExternal("npc_ownedbynpc",                 &GameScript::npc_ownedbynpc);
  bindExternal("npc_ownedbyguild",               &GameScript::npc_ownedbyguild);
```

`game/game/gamescript.cpp` (new function, placed next to `npc_ownedbynpc` near line 3084):

```cpp
// NOTE: in original-game Npc_OwnedByGuild @0x006e9ca0 -> oCItem::IsOwnedByGuild @0x007127c0
//       returns (guild > 0 && item.owner_guild == guild). Sibling of Npc_OwnedByNpc.
bool GameScript::npc_ownedbyguild(std::shared_ptr<zenkit::IItem> itmRef, int guild) {
  auto itm = findItem(itmRef.get());
  if(itm==nullptr)
    return false;
  return guild>0 && itm->handle().owner_guild==guild;
  }
```

Grep-verified symbols: `findItem` (used at gamescript.cpp:3077), `Item::handle()`
(item.h:88), `zenkit::IItem::owner_guild` (daedalus.hh:295). Argument order matches the original
`(item, guild)` and mirrors the existing `npc_ownedbynpc` binding style.
