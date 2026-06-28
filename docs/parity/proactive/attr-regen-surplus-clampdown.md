# Regen clamps surplus HP/Mana down to max (original preserves it)

**Confidence:** High (for the `v>=max` surplus case); the `chg<=0` no-drain half is also decomp-confirmed.

## Original fn + address
`oCNpc::ChangeAttribute` (Gothic2.exe `0x0072ff60`) clamps HP to `ATR_HITPOINTSMAX`
and Mana to `ATR_MANAMAX` only on the *upward* path (current + delta) and clamps the
floor to 0; for STRENGTH/DEXTERITY there is no upper bound. That clamp logic is mirrored
faithfully in OpenGothic, so there is **no** divergence inside `changeAttribute` itself.

The divergence lives in the regeneration path. `oCNpc::Regenerate` (Gothic2.exe
`0x00741fd0`) applies a per-interval regen tick (`ChangeAttribute(HITPOINTS,+1)` /
`ChangeAttribute(MANA,+1)`) guarded by **two** strict conditions for each pool:

- `0 < ATR_REGENERATE*` (the regen rate must be strictly positive — a negative rate
  never drains), and
- `current < max` (HP: `attr[0] < attr[1]`; Mana: `attr[2] < attr[3]`).

Because the tick fires only while `current < max` and only ever *adds*, the original
**never reduces a value that already sits at or above max**. This preserves the classic
Gothic mechanic: equip an item that raises `ATR_MANAMAX`/`ATR_HITPOINTSMAX`, let regen
fill the pool up to the boosted cap, then unequip — `ChangeAttribute(MANAMAX,-bonus)`
lowers the cap but leaves `current` untouched, so the surplus (`current > max`) survives
and can be spent. (Item max-boosts being applied at all is the recently-added
`AddItemEffects`/`RemoveItemEffects` `change_atr[]` path, inventory.cpp:896.)

## OG file:line
`/Users/admin/Downloads/opengothic/game/world/objects/npc.cpp:2503` (`Npc::tickRegen`),
called from npc.cpp:2594 (HP) and npc.cpp:2596 (Mana).

## Divergence
`tickRegen` early-returns only on `chg==0`, then unconditionally computes
`nextV = std::max(0, std::min(v+val1-val0, max))` and writes it back when it differs.

- When `v > max` (surplus after unequipping a +MAX item), with `chg>0` the
  `std::min(..., max)` collapses `nextV` to `max`, so the next regen tick silently
  **drains the surplus down to the base cap** — the opposite of the original, which keeps
  it (its `current < max` guard skips the tick entirely).
- When `chg < 0`, `tickRegen` accumulates a negative increment and **drains** the pool
  (the `// check health, in case of negative chg` branch). The original's `0 < REGENERATE*`
  guard means a negative regen rate is inert and never drains.

## Proposed patch
```cpp
// OLD (npc.cpp:2503)
void Npc::tickRegen(int32_t& v, const int32_t max, const int32_t chg, const uint64_t dt) {
  uint64_t tick = owner.tickCount();
  if(tick<dt || chg==0)
    return;
  int32_t time0 = int32_t(tick%1000);
  int32_t time1 = time0+int32_t(dt);

  int32_t val0 = (time0*chg)/1000;
  int32_t val1 = (time1*chg)/1000;

  int32_t nextV = std::max(0,std::min(v+val1-val0,max));
  if(v!=nextV) {
    v = nextV;
    // check health, in case of negative chg
    checkHealth(true,false);
    }
  }

// NEW
void Npc::tickRegen(int32_t& v, const int32_t max, const int32_t chg, const uint64_t dt) {
  uint64_t tick = owner.tickCount();
  // NOTE: in original-game oCNpc::Regenerate @0x00741fd0 a regen tick fires only while the
  // regen rate is strictly positive (0 < ATR_REGENERATE*) AND current < max, and only ever
  // adds toward the cap (ChangeAttribute(+1)). It never pulls a value that already sits at or
  // above max back down, so a surplus left after unequipping a +HITPOINTSMAX/+MANAMAX item is
  // preserved (regen to the boosted cap -> unequip -> keep the surplus). A negative regen rate
  // is inert and never drains. OpenGothic's unconditional min(v+..,max) clamped the surplus
  // down on the next tick, and a negative chg drained the pool.
  if(tick<dt || chg<=0 || v>=max)
    return;
  int32_t time0 = int32_t(tick%1000);
  int32_t time1 = time0+int32_t(dt);

  int32_t val0 = (time0*chg)/1000;
  int32_t val1 = (time1*chg)/1000;

  int32_t nextV = std::min(v+val1-val0,max);
  if(v!=nextV)
    v = nextV;
  }
```
The `checkHealth` call is dropped because, with regen now strictly additive (matching the
original), a regen tick can never lower HP and therefore can never cause death/unconsciousness.
