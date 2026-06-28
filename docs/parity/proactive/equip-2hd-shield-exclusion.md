# Equip parity: two-handed weapon does not unequip the shield

**Confidence:** High

## Original fn + address

`oCNpc::Equip` (Gothic2.exe `0x00739c90`) routes a melee weapon (`GetCategory==1`,
flag `ITM_CAT_NF` bit `0x2`) to `oCNpc::EquipWeapon` (`0x0073a030`). EquipWeapon, after
the `CanUse` (`0x007319b0`) stat-gate passes, branches on the two-handed flags
`ITM_2HD_SWD` (`0x10000`) and `ITM_2HD_AXE` (`0x20000`):

- One-handed melee weapon (`ITM_CAT_NF` set, not a shield): it unequips the items found
  in the two **melee** weapon node slots only (the node-name globals `DAT_00ab1ef4` and
  `DAT_00ab1f64`, the same two nodes scanned by `oCNpc::GetEquippedMeleeWeapon`
  `0x00737930`), then `EquipItem`. The shield is left untouched.
- **Two-handed** melee weapon (`ITM_2HD_SWD` or `ITM_2HD_AXE`): it unequips those two
  melee nodes **and additionally the SHIELD node** (`DAT_00ab1e10`, confirmed as the
  shield node by `oCNpc::ShieldEquipped` `0x00737e50`), then `EquipItem`.

So a two-handed weapon and a shield are mutually exclusive: equipping a 2H weapon drops
the shield; equipping a 1H weapon keeps it. The shield is removed only on the path where
the equip actually proceeds (for the player, `CanUse==0` aborts via `DisplayCannotUse`
before any unequip).

## OG file:line

`game/game/inventory.cpp:937-938` (the `ITM_CAT_NF` melee branch of `Inventory::use`):

```cpp
  if(mainflag & ITM_CAT_NF)
    return setSlot(melee,it,owner,force);
```

In OpenGothic `melee` and `shield` are fully independent slots (`inventory.h:172`).
`Inventory::setSlot` only ever touches the single slot it is handed, and nothing in
`use()` / `setSlot()` / `updateShieldView()` removes the shield when a melee weapon is
equipped. There is no two-handed/shield mutual-exclusion anywhere in the equip path.

## Divergence

Equipping a two-handed sword/axe while a shield is equipped: the original unequips the
shield; OpenGothic keeps **both** the 2H weapon and the shield equipped. The retained
shield continues to apply its `protection[]` bonus (via `applyArmor`) and its mesh,
neither of which is valid while wielding a two-handed weapon. Observable by the player in
the inventory UI and on combat NPCs scripted to wield 2H weapons.

## Proposed patch

`game/game/inventory.cpp`, melee branch of `Inventory::use`:

OLD:
```cpp
  if(mainflag & ITM_CAT_NF)
    return setSlot(melee,it,owner,force);
```

NEW:
```cpp
  if(mainflag & ITM_CAT_NF) {
    // NOTE: in original-game oCNpc::EquipWeapon (Gothic2.exe 0x0073a030) equipping a
    // two-handed melee weapon (ITM_2HD_SWD/ITM_2HD_AXE) also unequips the shield
    // (NPC_NODE_SHIELD slot) -- a two-handed weapon and a shield are mutually exclusive,
    // whereas a one-handed weapon keeps the shield. The shield is dropped only on the path
    // where the equip proceeds (after CanUse passes), so mirror setSlot's stat-gate before
    // removing it.
    if(it->is2H() && shield!=nullptr && (force || it->checkCond(owner)))
      setSlot(shield,nullptr,owner,false);
    return setSlot(melee,it,owner,force);
    }
```

`Item::is2H()` (`item.cpp:286`, tests `ITM_2HD_SWD|ITM_2HD_AXE`), `Item::checkCond`
(`item.h:84`, = `checkCondUse && checkCondRune`, matching setSlot's own gate), the
`shield` member (`inventory.h:172`) and `setSlot(slot,nullptr,...)` (the unequip form used
throughout, e.g. `inventory.cpp:606`) all exist and are verified.

Gating the shield removal on `force || it->checkCond(owner)` removes the shield exactly
when the subsequent `setSlot(melee,...)` will succeed, so no inconsistent state arises if
the weapon's own requirement is unmet (the player gets the G_CanNotUse refusal and the
shield stays on, as in the original).
