# Ammo parity: infinite-munition weapon flag (bit 25 / 0x2000000) is unimplemented

**Confidence:** Medium (engine-level divergence is certain; vanilla-observability and a
fully-faithful fix carry caveats — see Proposed patch).

## Original function + address (prose only)

Three engine functions all branch on the same per-item flag, tested via
`oCItem::HasFlag` @ `0x007126d0` (returns `(item.flags & arg) == arg`, reading the
runtime flags word at item offset `0x158`). The argument used in the munition path is
`0x2000000`, i.e. **bit 25** of the item flags. In `oCItem::InitByScript` @ `0x00711bd0`
the runtime flags word (`0x158`) is OR-ed with the script `C_Item.flags` field (`0x154`),
so bit 25 is reachable from script-side item data (the same `flags` field OpenGothic
exposes as `Item::itemFlag()` / `hitem->flags`).

- `oCNpc::IsMunitionAvailable` @ `0x0073c6e0`: after the null check it calls
  `HasFlag(0x2000000)` on the **weapon**; if set it returns "available" (1) immediately,
  *before* ever consulting the inventory count. Only when the flag is clear does it fall
  through to the inventory lookup (and, for the player, the "no ammo" notification).
  This function is the central availability gate — xrefs include `FightAttackBow`,
  `BowMode`, `HasRangedWeaponAndAmmo`, `EquipBestWeapon`, `InitAim`.
- `oCNpc::DoInsertMunition` @ `0x00744190` (driven by the `DEF_INSERT/PLACE_MUNITION`
  anim event when the bow is drawn/loaded): if the weapon flag is clear it *removes one*
  munition instance from the inventory and holds it; if the flag is **set** it instead
  *spawns a fresh transient* munition item (item factory, instance = weapon munition id)
  and does **not** touch the inventory. This is the "create-ammo on draw" behavior.
- `oCNpc::DoRemoveMunition` @ `0x00744470` (driven by the remove-munition anim event when
  the bow is lowered without firing): if the weapon flag is clear it returns the held
  munition to the inventory; if the flag is **set** it just destroys the transient item.

Net effect: a ranged weapon with item flag bit 25 set has **infinite ammo** — it is always
shootable, never depletes the inventory, and generates its own projectile each draw.

Note: bit 25 (`0x2000000`) is distinct from OpenGothic's `ITM_MULTI = 1 << 21`
(constants.h:345), which models *stackability* and is unrelated to this gate.

## OpenGothic file:line

- `game/world/objects/npc.cpp:4234` `Npc::hasAmmunition()` — only checks
  `munition<0 || invent.itemCount(munition)<=0`; no weapon-flag bypass.
- `game/world/objects/npc.cpp:4191` `Npc::shootBow()` — fetches `invent.getItem(munition)`
  (null when inventory count is 0 → returns false) and unconditionally calls
  `invent.delItem(munition,1,*this)` on every shot.
- No create-ammo / infinite-ammo / create-on-draw logic exists anywhere in `game/`
  (grep for `createammo|infinite|generate ammo` finds nothing in the ammo path).

## Divergence

In OpenGothic a ranged weapon that, in vanilla, carries the infinite-munition flag
(item flag bit 25) behaves like an ordinary weapon: it consumes one arrow/bolt from the
inventory per shot and refuses to fire once the inventory count reaches zero. In the
original engine such a weapon never consumes inventory ammo, fabricates its own
projectile on draw, and `IsMunitionAvailable` reports it shootable regardless of stock.

## Proposed patch — DEFERRED (partial fix possible, full parity is not surgical)

A minimal, faithful gate is straightforward and grep-verified against existing symbols:

`game/world/objects/npc.cpp:4234` `Npc::hasAmmunition()`
```
OLD:
  const int32_t munition = active->handle().munition;
  if(munition<0 || invent.itemCount(size_t(munition))<=0)
    return false;
  return true;
NEW:
  // NOTE: in original-game oCNpc::IsMunitionAvailable @0073c6e0 a weapon whose item
  // flag bit 25 (0x2000000) is set always reports munition available, bypassing the
  // inventory count (oCItem::HasFlag @007126d0; flag merged from C_Item.flags in
  // oCItem::InitByScript @00711bd0).
  if(uint32_t(active->handle().flags) & 0x2000000u)
    return true;
  const int32_t munition = active->handle().munition;
  if(munition<0 || invent.itemCount(size_t(munition))<=0)
    return false;
  return true;
```
and skip the deduction in `shootBow()` (`game/world/objects/npc.cpp:4215`) when the same
flag is set (mirroring `DoRemoveMunition`'s "destroy transient, don't return to inv" arm).

**Why DEFERRED rather than applied:**
1. **Out-of-stock firing is not covered surgically.** `shootBow()` obtains the projectile
   via `invent.getItem(munition)`, which is `nullptr` when the inventory holds zero of the
   ammo. The original handles this by *spawning a transient munition item* in
   `DoInsertMunition` to act as the projectile. Faithfully supporting an infinite weapon
   with an empty quiver therefore requires item-instantiation plumbing
   (create item by instance, copy damage, use it as the bullet), which is a non-trivial
   change to `Inventory`/`World::shootBullet`, not a one-line gate. The minimal patch above
   only fixes the case where the weapon flag is set *and* the player still carries ammo.
2. **Vanilla observability unverified.** Bit 25 is reachable from `C_Item.flags`, but it is
   not in the standard Constants.d names and I could not confirm a vanilla Gothic II item
   that sets it (no script data in this repo). Per "empty beats false positives," enabling
   an infinite-ammo bypass should be gated on confirming at least one shipped item uses
   bit 25 (or that mods rely on it), so the flag-detection branch is exercised and cannot
   silently grant infinite ammo to an item that set bit 25 for another reason.

Recommended next step before applying: dump the vanilla G2 item instances and check whether
any ranged weapon's `flags` has bit 25 set; if yes, land the full create-on-draw path.
