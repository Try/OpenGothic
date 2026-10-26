# Gamepad support

OpenGothic uses Tempest's gamepad API for native macOS input through Apple's
GameController framework. Connect a DualSense over Bluetooth before or during
the game. One controller is active at a time; keyboard and mouse remain usable.
After a focus change, disconnect, or screen transition, release held controls
and return the sticks to neutral before continuing.

## Default controls

Movement follows Gothic's original directional movement and animations.

| Control | Gameplay action |
|---|---|
| Left stick | Move |
| Right stick | Camera |
| Cross / R2 | Interact or attack |
| Circle | Jump / swim action |
| Square | Draw or put away weapon |
| Triangle | Inventory |
| D-pad Up / Down | Melee / ranged weapon |
| D-pad Left / Right | Previous / next assigned spell |
| L1 / R1 | Left / right attack |
| L2 | Block |
| L3 / R3 | Walk / sneak |
| Create | Journal |
| Options | Pause / back |

Menus use the D-pad or left stick, Cross to confirm, and Circle or Options to
go back. Square opens controller settings while a menu is open; F1 provides
keyboard access to the same screen. Settings include separate movement and
camera dead zones, X/Y camera sensitivity, inverted Y, hints, and gameplay
button assignments. UI navigation stays fixed. Apply commits the draft to
`Gothic.ini` under `[GAMEPAD] profile`; Cancel discards it.

In inventory and trade, Square opens the quantity dialog and L1/R1 switch
panels. The dialog supports a selected amount or the full stack. Triangle
assigns a spell slot when applicable. The save-name editor provides a
controller keyboard with explicit confirm/cancel, including Russian names.
Held confirmation does not repeat an item use or trade transaction.

With G1-style controls, a fresh R2 press waits for the bow to become ready and
fires once while held; release and press again for the next shot. An early
release or entering a menu cancels the pending shot. Keyboard modifiers and
direct controller attacks retain separate ownership in mixed input.
`-g2` selects Gothic II: Night of the Raven game data; it does not select the
`GAME/useGothic1Controls` setting.

## Build and automated checks

The Tempest submodule must contain the gamepad API/backend and event-loop
boundary used by this change. Use the pinned submodule commit rather than an
unmodified older Tempest checkout:

Follow the normal platform build prerequisites. If packaging has not generated
`game/build.h`, create that local version header before building:

```cpp
#pragma once
static const char* appBuild = "OpenGothic local";
```

```sh
git submodule update --init --recursive
cmake -S . -B build -DOPENGOTHIC_GAMEPAD_TOOLS=ON
cmake --build build --target Gothic2Notr Spacer GamepadInputTests GamepadStateTests MacOSGamepadTests InstanceStorageTests PlayerControlInputTests PlayerControlBowTests InventoryMenuMergeTests GamepadDialogTests GamepadMenuLoopTests TempestGamepadProbe --parallel 6
ctest --test-dir build --output-on-failure
```

The target list above is for macOS with Clang.
`OPENGOTHIC_GAMEPAD_TOOLS` defaults to `OFF`; the test framework is already
vendored in Tempest, and no game assets are needed for the automated checks.
The macOS backend lifetime fixture uses AddressSanitizer with Clang.
PlayerControl fixtures generate method bodies from production sources so
input and bow regressions cannot silently test an outdated copied router.
The inventory merge fixture also executes production iterator, page, selection,
row-count and scrolling methods, including separate equipped-item slots and
safe-area/scaling boundaries. Native dialog checks cover inset changes and
visible keyboard/click geometry without changing save confirm/cancel decisions.

`build/opengothic/TempestGamepadProbe --seconds 180` opens a diagnostic window
and writes device events and axes to stdout. Focus its window, then exercise
buttons, sticks, triggers, focus loss, and reconnect. Check the transport
separately in macOS HID information; a filename or controller name does not
establish Bluetooth or USB.

## Acceptance scope

The accepted runtime scope is macOS arm64 with a DualSense over Bluetooth,
Gothic II: Night of the Raven, and G1-style controls. Physical checks covered
menu navigation, movement and camera, melee hits, bow shots with ammunition
consumption, spell cycling/casting, sneak, a real ladder, swimming/diving,
lockpicking, inventory, controlled trade, save cancel/confirm/load, mixed
keyboard/mouse input, focus loss, reconnect, and normal exit. Save checks used
isolated profiles and verified that the original files were unchanged.

Those physical observations belong to the earlier feature build. The revision
synchronized with upstream has fresh build and automated regression coverage;
a new physical controller session was not performed for that merged revision.

Camera control was checked qualitatively at approximately 30 and 42–45
observed engine FPS. Physical USB and sustained actual 60/120 FPS checks are
excluded from acceptance and remain unverified. Automated camera formula
checks at 30/60/120 FPS and stalled-frame clamping remain enabled.

Other controller models, G2-style controls, crossbows, and every item/NPC UI
variant were not physically validated. Non-macOS platforms currently expose
an empty gamepad backend. Rumble and free 360-degree movement are outside
this implementation. Full-engine sanitizer and non-macOS build coverage
must be distinguished from the targeted fixtures and macOS checks above.
