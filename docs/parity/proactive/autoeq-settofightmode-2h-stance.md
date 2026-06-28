# Npc_SetToFightMode forces W1H/Fist by guild in the original, not W2H by weapon

**Confidence:** High (for the human 2H→1H stance divergence; the monster→Fist part shares the same branch and decompile evidence).

## Original function + address

The Daedalus external `Npc_SetToFightMode` is implemented at `0x006e6f10` in `Gothic2.exe`
(`P:\dev\g2addon\release\Gothic\_ulf\oGameExternal.cpp`). Its spawn-into-combat behaviour is:

1. If the NPC's `ZS_RIGHTHAND` node already holds an `oCItem`, it does nothing (right-hand-occupied guard).
2. Otherwise it creates the weapon instance, `oCNpc::PutInInv`s it, and `oCNpc::PutInSlot(..,"ZS_RIGHTHAND",..)`
   puts the weapon **directly into the right hand** (drawn).
3. It then calls the virtual `oCNpc::SetWeaponMode` (`0x00739940`, vtable +0x9c) with a **fixed mode
   chosen purely by guild**:
   - `GetTrueGuild()` (falling back to `GetGuild()` when the true guild is 0); if that guild
     `< 0x10` (`GIL_SEPERATOR_HUM` = 16) → `SetWeaponMode(3)` = **W1H**;
   - otherwise (monsters/orcs, guild ≥ 16) → `SetWeaponMode(1)` = **Fist**.

`SetWeaponMode` @0x00739940 merely stores the passed mode into the npc weapon-mode field (engine
offset 0x250, clamped 0..7) and updates camera/focus — it does **not** re-derive the mode from the
weapon. So the original **never inspects whether the weapon is two-handed**: a human spawned
ready-to-fight with a 2H weapon via `Npc_SetToFightMode` ends up in the **1H stance/animation set
(mode 3)**, and any guild ≥ 16 ends up in **Fist (mode 1)**.

## OG file:line

`/Users/admin/Downloads/opengothic/game/world/objects/npc.cpp:3657` — `Npc::setToFightMode`
(bound to the `npc_settofightmode` external in `game/game/gamescript.cpp:159`, lines 3668–3680).

## Divergence

OpenGothic keys the fight stance on the **weapon** rather than the **guild**:

```cpp
auto weaponSt = WeaponState::W1H;
if(w->is2H()) { weaponSt = WeaponState::W2H; } else { weaponSt = WeaponState::W1H; }
...
hnpc->weapon = (st==WeaponState::W1H ? 3:4);   // 4 = W2H for two-handed weapons
```

So:
- A **human** scripted into combat with a **2H weapon** gets `WeaponState::W2H` / `hnpc->weapon == 4`,
  whereas the original always uses W1H (mode 3) for guild < 16.
- A **monster/orc** (guild ≥ 16) gets W1H/W2H from OG, whereas the original sets **Fist (mode 1)**.

Net effect: wrong fight stance and animation set (and wrong `C_Npc.weapon` value seen by scripts) for
NPCs spawned ready-to-fight via `Npc_SetToFightMode`.

## Proposed patch

`game/world/objects/npc.cpp`, `Npc::setToFightMode` (lines 3668–3680):

OLD:
```cpp
  auto weaponSt = WeaponState::W1H;
  if(w->is2H()) {
    weaponSt = WeaponState::W2H;
    } else {
    weaponSt = WeaponState::W1H;
    }

  if(visual.setToFightMode(weaponSt))
    updateWeaponSkeleton();

  auto& weapon = *currentMeleeWeapon();
  auto  st     = weapon.is2H() ? WeaponState::W2H : WeaponState::W1H;
  hnpc->weapon  = (st==WeaponState::W1H ? 3:4);
```

NEW:
```cpp
  // NOTE: in original-game the Npc_SetToFightMode external @0x006e6f10 puts the weapon into
  // ZS_RIGHTHAND and calls oCNpc::SetWeaponMode @0x00739940 with a FIXED mode chosen only by guild:
  // human guild (trueGuild < GIL_SEPERATOR_HUM/0x10) -> mode 3 (W1H), otherwise -> mode 1 (Fist).
  // It never inspects is-two-handed, so a human spawned ready-to-fight with a 2H weapon uses the
  // 1H stance/animations (not W2H). OG keyed the stance on is2H (mode 4 for 2H), diverging from the
  // original spawn-into-combat fight mode.
  const bool human    = isHuman();
  const auto weaponSt = human ? WeaponState::W1H : WeaponState::Fist;

  if(visual.setToFightMode(weaponSt))
    updateWeaponSkeleton();

  hnpc->weapon = (human ? 3 : 1);
```

(`isHuman()` @ `npc.cpp:1367` already computes `trueGuild() < GIL_SEPERATOR_HUM`, the same threshold
the original uses; `WeaponState::Fist` is already used at `npc.cpp:3685`.)

### Notes / scope
- The original's separate right-hand-occupied early-out and its direct `PutInSlot(ZS_RIGHTHAND)` (vs
  OG routing through the inventory melee slot) are left unchanged here; this patch only corrects the
  resolved fight mode, which is the clearly observable behavioural divergence.
- The ranged-weapon case (OG's `currentMeleeWeapon()!=item` early return at line 3665 skips non-melee
  items entirely) is a separate, narrower divergence and is out of scope for this fix.
