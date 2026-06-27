# Parity: `Npc_IsDrawingWeapon` returns weapon-vs-spell backwards (inverted `isSpell()` test)

**Confidence:** High

## Original function + address

`Npc_IsDrawingWeapon` handler is `FUN_006f7be0` (Gothic2.exe `0x006f7be0`); the sibling
`Npc_IsDrawingSpell` handler is `FUN_006f7d80` (`0x006f7d80`). Both walk the NPC's
EventManager message queue. `Npc_IsDrawingWeapon` returns a boolean (default `0`) that is
set to `1` when it finds an active **weapon** draw message (an `oCMsgWeapon` whose subtype
is below 3, i.e. a draw/equip message) — it is the *weapon* branch. `Npc_IsDrawingSpell`
(default sentinel `-1`) instead returns the spell id of an active `oCMsgMagic` draw message —
the *spell* branch. So in the original the two handlers test opposite message classes: one
keys on the weapon message, the other on the magic message.

## OpenGothic file:line

`game/game/gamescript.cpp:2713` (`GameScript::npc_isdrawingweapon`), compared against
`GameScript::npc_isdrawingspell` at `game/game/gamescript.cpp:2700`.

## Divergence

The two OpenGothic handlers are byte-for-byte identical in their predicate — both bail out
unless the active weapon **is a spell** and then return that spell's `clsId`:

```cpp
// npc_isdrawingspell  (2700)            // npc_isdrawingweapon (2713)
auto ret = npc->activeWeapon();          auto ret = npc->activeWeapon();
if(ret==nullptr || !ret->isSpell())      if(ret==nullptr || !ret->isSpell())
  return 0;                                return 0;
makeCurrent(ret);                        makeCurrent(ret);
return int32_t(ret->clsId());            return int32_t(ret->clsId());
```

This is a copy-paste of the spell handler. Consequences for `npc_isdrawingweapon`:
- An NPC drawing an actual melee/ranged **weapon** (`activeWeapon()->isSpell()==false`)
  returns `0` (false) — exactly the case the function is named for.
- An NPC drawing a **spell** returns the spell `clsId` — i.e. it fires for the wrong class,
  duplicating `Npc_IsDrawingSpell`.

The `!` in the active-weapon branch is inverted: the weapon handler must accept the active
item when it is **not** a spell. `isSpell()`, `activeWeapon()`, and `clsId()` are all
grep-verified (`game/world/objects/item.h:71`, `item.h:91`, `npc.h:341`).

## Proposed patch

```cpp
int GameScript::npc_isdrawingweapon(std::shared_ptr<zenkit::INpc> npcRef) {
  auto npc = findNpc(npcRef);
  if(npc==nullptr)
    return 0;

  auto ret = npc->activeWeapon();
  // NOTE: in original-game Npc_IsDrawingWeapon @0x006f7be0 the weapon handler keys on the
  // weapon draw message (oCMsgWeapon), unlike Npc_IsDrawingSpell @0x006f7d80 which keys on
  // the magic message; OpenGothic had the spell test inverted, making this a duplicate of
  // npc_isdrawingspell (returned 0 for an actual weapon and the spell id for a spell).
  if(ret==nullptr || ret->isSpell())
    return 0;

  makeCurrent(ret);
  return int32_t(ret->clsId());
  }
```

Only the predicate `!ret->isSpell()` becomes `ret->isSpell()` (drop the `!`), so the function
returns the drawn weapon's `clsId` for a real weapon and `0` otherwise — non-zero/zero, which
matches the original's truthy/`0` boolean contract for the common `if(Npc_IsDrawingWeapon(...))`
script usage. Surgical and build-verifiable.

## Externals checked and found faithful

- `Wld_GetPlayerPortalGuild` / `Wld_GetFormerPlayerPortalGuild` / `Npc_GetPortalGuild` — all
  default to `GIL_NONE` and resolve via `guildOfRoom(portalName())`; consistent.
- `Wld_IsFPAvailable` / `Wld_IsNextFPAvailable` — `false` on null self, else pointer test.
- `Npc_IsInPlayersRoom` — `false` default, compares portal names.
- `Npc_GetReadiedWeapon` — returns `nullptr` (0) handle when no active weapon.
- `Npc_HasReadiedWeapon` / `...MeleeWeapon` / `...RangedWeapon` — correct `WeaponState`
  partitions, `false` on null.
- `Npc_GetLastHitSpellID` — `0` on null. `Npc_GetLastHitSpellCat` — `SPELL_GOOD` on null,
  else `spellDesc(id).spell_type`.
- `Npc_GetActiveSpellLevel` — null/edge handling matches the previously-fixed `+1` form.
- `Npc_GetDetectedMob` — empty string default; `Npc_IsInCutscene`, `Npc_IsInRoutine`,
  `Npc_HasItems`, `Npc_HasSpell`, `Hlp_IsItem` all return faithful zero/false defaults.
- `Npc_GetComrades` is not bound in this build (no divergence to assess).

One subtlety left intentionally untouched (not part of this fix): the original
`Npc_IsDrawingSpell` default sentinel is `-1`, whereas OpenGothic's `npc_isdrawingspell`
returns `0`. Scripts use it via equality against a concrete spell id, so both sentinels fail
to match equally; flagged here but DEFERRED as low-impact and separate from the weapon bug.
