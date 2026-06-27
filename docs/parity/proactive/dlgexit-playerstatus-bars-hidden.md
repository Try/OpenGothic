# Dialog exit-cleanup: player HP/Mana/Swim status bars are not hidden during conversation (and so never "restored" on dialog end)

**Confidence:** High (divergence verified against the binary; fix is surgical and build-verifiable).

## Original function + address (prose only)
The conversation lifecycle in `Gothic2.exe` brackets the on-screen player status display with two calls:

- At dialog **start**, `oCInformationManager::SetNpc` @ `0x006609f0` calls `oCGame::SetShowPlayerStatus(0)` (after `SetMovLock(player,1)` and `CloseInventory(player)`).
- At dialog **termination/exit-cleanup**, `oCInformationManager::OnTermination` @ `0x006631a0` calls `oCGame::SetShowPlayerStatus(1)` (alongside restoring the camera to `CamModNormal` and `SetMovLock(player,0)`), i.e. it *restores* the status display as part of the same end-of-conversation state restore.

`oCGame::SetShowPlayerStatus(int)` @ `0x006c2d90`, when passed `0`, removes the four player-status screen views (the HP bar, the Mana bar, and two further bars at object offsets `0x8c/0x90/0x94/0x98`) from `screen` and stores the flag at offset `0x9c`. `oCGame::GetShowPlayerStatus` @ `0x006c2df0` just returns that `0x9c` flag, and `oCGame::UpdatePlayerStatus` @ `0x006c3140` early-returns at its top when `0x9c == 0` — so while a conversation is active the per-frame status update re-adds/draws nothing. Net effect in the original: the player's HP/Mana/swim bars vanish for the entire duration of a conversation and reappear only when the dialog manager reaches `OnTermination`.

## OpenGothic file:line
`game/mainwindow.cpp:223-237` — the player status-bar block inside `MainWindow::paintEvent`.

```
bool showHealthBar = opt.showHealthBar;
...
bool showManaBar   = (pl->attribute(ATR_MANAMAX)>0) &&
                     ((opt.showManaBar==2) || (opt.showManaBar==1 && (pl->weaponState()==WeaponState::Mage || inventory.isActive())));
bool showSwimBar   = (opt.showSwimBar==2) || (opt.showSwimBar==1 && pl->isDive());

if(showHealthBar) drawBar(p,barHp, ...);
if(showManaBar)   drawBar(p,barMana, ...);
if(showSwimBar)   { ... drawBar(p,barMisc, ...); }
```

Grep-verified symbols: `MainWindow::dialogs` is a `DialogMenu` member (`game/mainwindow.h:149`); `DialogMenu::isActive()` exists (`game/ui/dialogmenu.h:34`, defined `game/ui/dialogmenu.cpp:265` as `(state!=State::Idle) || current.time>0`) and is already used as the in-conversation predicate elsewhere in this same file (`game/mainwindow.cpp:443,553,575,787,932,965,1000,1082`). `opt.showHealthBar/showManaBar/showSwimBar`, `drawBar`, `barHp/barMana/barMisc` all present at the cited lines.

## Divergence
OpenGothic draws the player HP/Mana/swim bars in `MainWindow::paintEvent` purely from the user options (`opt.showHealthBar` etc.), with **no in-dialog guard**. Because `paintEvent` keeps rendering the world HUD underneath the dialog overlay (it still calls `drawMsg`/`paintFocus` in the same branch), the bars remain on screen for the whole conversation. The original engine deliberately removes them at conversation start (`SetShowPlayerStatus(0)` in `SetNpc`) and restores them as part of dialog exit-cleanup (`SetShowPlayerStatus(1)` in `OnTermination`). Result: in `Gothic2.exe` the corner HP/Mana bars disappear while you talk to an NPC and pop back when the dialog ends; in OpenGothic they stay visible throughout. The existing code comment at `game/mainwindow.cpp:224-227` already cites `oCGame::UpdatePlayerStatus @0x006c3140` for the MANAMAX gate but overlooked that the same routine is short-circuited entirely (`0x9c==0`) while a dialog is open.

## Proposed patch
Gate the three bars on "not currently in a conversation", mirroring the original's `GetShowPlayerStatus()` flag (cleared for the whole dialog window, set again at `OnTermination`). `DialogMenu::isActive()` is the OG-side equivalent of that flag's dialog-driven lifetime.

OLD (`game/mainwindow.cpp:223-230`):
```cpp
          bool showHealthBar = opt.showHealthBar;
          // NOTE: in original-game oCGame::UpdatePlayerStatus @0x006c3140 the mana bar is shown only
          // when GetAttribute(player, ATR_MANAMAX) > 0 (in addition to the stance/inventory check); a
          // MANAMAX==0 hero (default fresh start) never gets a mana bar, and gating here also avoids
          // the MANA/MANAMAX == 0/0 == NaN fill fraction.
          bool showManaBar   = (pl->attribute(ATR_MANAMAX)>0) &&
                               ((opt.showManaBar==2) || (opt.showManaBar==1 && (pl->weaponState()==WeaponState::Mage || inventory.isActive())));
          bool showSwimBar   = (opt.showSwimBar==2) || (opt.showSwimBar==1 && pl->isDive());
```

NEW:
```cpp
          // NOTE: in original-game oCInformationManager::SetNpc @0x006609f0 calls SetShowPlayerStatus(0)
          // at dialog start and oCInformationManager::OnTermination @0x006631a0 calls SetShowPlayerStatus(1)
          // on dialog exit-cleanup; oCGame::UpdatePlayerStatus @0x006c3140 early-returns while that flag is
          // 0, so the HP/Mana/swim bars are hidden for the whole conversation. dialogs.isActive() mirrors
          // that flag's dialog-driven lifetime.
          const bool inDialog = dialogs.isActive();

          bool showHealthBar = opt.showHealthBar && !inDialog;
          // NOTE: in original-game oCGame::UpdatePlayerStatus @0x006c3140 the mana bar is shown only
          // when GetAttribute(player, ATR_MANAMAX) > 0 (in addition to the stance/inventory check); a
          // MANAMAX==0 hero (default fresh start) never gets a mana bar, and gating here also avoids
          // the MANA/MANAMAX == 0/0 == NaN fill fraction.
          bool showManaBar   = !inDialog && (pl->attribute(ATR_MANAMAX)>0) &&
                               ((opt.showManaBar==2) || (opt.showManaBar==1 && (pl->weaponState()==WeaponState::Mage || inventory.isActive())));
          bool showSwimBar   = !inDialog && ((opt.showSwimBar==2) || (opt.showSwimBar==1 && pl->isDive()));
```
