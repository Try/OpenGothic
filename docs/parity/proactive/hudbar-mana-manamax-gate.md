# HUD mana status-bar missing `MANAMAX > 0` show-gate

**Confidence:** High

## Original function + address
`oCGame::UpdatePlayerStatus` @ `0x006c3140` (`P:\dev\g2addon\release\Gothic\_ulf\oGame.cpp`).

In the original, the player mana status-bar (the `screen` view item at this+0x94) is
inserted/updated only when **both** of two conditions hold:

1. the inventory/spell interface is open **OR** the player's weapon mode is MAGIC
   (`GetWeaponMode(player) == 7`), and
2. `oCNpc::GetAttribute(player, 3) > 0`, i.e. attribute index 3 = `ATR_MANAMAX` is
   strictly positive.

Only after that gate does it call the status-bar's `SetRange(0, MANAMAX)` /
`SetValue(MANA)`. So a hero with `MANAMAX == 0` (the default state of a fresh Gothic II
character, who has no mana until learning a circle of magic / drinking mana potions)
*never* shows a mana bar in the original game, regardless of inventory/stance.

## OpenGothic file:line
`/Users/admin/Downloads/opengothic/game/mainwindow.cpp:221` and `:224`

```cpp
float mp  = float(pl->attribute(ATR_MANA)) / float(pl->attribute(ATR_MANAMAX));
...
bool showManaBar = (opt.showManaBar==2) ||
                   (opt.showManaBar==1 && (pl->weaponState()==WeaponState::Mage || inventory.isActive()));
```

## Divergence
OpenGothic's `showManaBar` condition omits the original's `MANAMAX > 0` gate. With the
default SystemPack setting `ShowManaBar=1` (mode 1 → "show in mage stance or with
inventory open"), opening the inventory on a character with `MANAMAX == 0` displays a
mana bar that the original never draws. Worse, the fill fraction is computed as
`mp = MANA / MANAMAX = 0 / 0 = NaN`, so the bar is not just spuriously present but is
filled from a NaN value (`drawBar` clamps with `std::max(0,std::min(NaN,1))`, which is
implementation-defined and renders a garbage/empty rect). This is the mana-bar analogue
of the already-fixed focus-HP-bar `attribute > 0` gate, but it targets a different bar
(player mana) and a different attribute (`ATR_MANAMAX`, index 3).

## Proposed patch
Gate `showManaBar` on `MANAMAX > 0`, matching the original's second condition. This both
restores the show/hide parity and prevents the NaN fill.

OLD (`game/mainwindow.cpp:224`):
```cpp
          bool showManaBar   = (opt.showManaBar==2) || (opt.showManaBar==1 && (pl->weaponState()==WeaponState::Mage || inventory.isActive()));
```

NEW:
```cpp
          // NOTE: in original-game oCGame::UpdatePlayerStatus @0x006c3140 the mana bar is shown only
          // when GetAttribute(player, ATR_MANAMAX) > 0 (in addition to the stance/inventory check);
          // a MANAMAX==0 hero (default fresh start) never gets a mana bar, and gating here also
          // avoids the MANA/MANAMAX == 0/0 == NaN fill fraction.
          bool showManaBar   = (pl->attribute(ATR_MANAMAX)>0) &&
                               ((opt.showManaBar==2) || (opt.showManaBar==1 && (pl->weaponState()==WeaponState::Mage || inventory.isActive())));
```

Symbols verified to exist: `Npc::attribute(Attribute)` (`game/world/objects/npc.h:216`),
`ATR_MANAMAX = 3` (`game/game/constants.h:476`), `InventoryMenu::isActive()`
(`game/ui/inventorymenu.h:49`), `WeaponState::Mage` (used already at this call site).
