# Camera: spell-casting state selects ranged-cam instead of magic-cam

**Confidence:** High

## Original function + address

`oCAIHuman::ChangeCamModeBySituation` @ `0x0069cd00` (P:\dev\g2addon\release\Gothic\_ulf\oAiHuman.cpp).

This is the authoritative per-tick routine that maps the player's situation to a
`CamMod*` mode name, then calls `zCAICamera::SetMode` @ `0x004a09c0`. After the
earlier death/unconscious, mob-interaction, dive, swim, climb and fall branches,
the final weapon-state block (around `0x0069d6e6`) reads
`oCNpc::GetWeaponMode` and selects:

- weapon mode `== 7` (spell/magic) -> `CamModMagic`
- weapon mode `== 5` or `== 6` (bow / crossbow) -> `CamModRanged`
- weapon mode in `1..4` (fist / 1H / 2H / dagger) -> `CamModMelee`
- otherwise (`0` = no weapon, or out of range) -> `CamModNormal`
- (an earlier inventory predicate overrides all of the above with `CamModInventory`)

So in the original, the spell-casting state unconditionally drives the dedicated
`CAMMODMAGIC` camera instance (its own bestRange/min/max/rotation defaults), and
is kept distinct from the `CAMMODRANGED` bow/crossbow camera.

## OpenGothic file:line

`game/mainwindow.cpp:1016-1028` (`MainWindow::solveCameraMode`).

The mode-to-CamMod-instance wiring is `game/camera.cpp:528-565`
(`Camera::cameraDef`), where `camMod==Magic` -> `camd.mageCam()` and
`camMod==Ranged` -> `camd.rangeCam()`. `Camera::Magic` exists
(`game/camera.h:26`) and `mageCam()` is defined in
`game/game/definitions/cameradefinitions.h:16`.

## Divergence

OpenGothic maps the magic weapon-state to the ranged camera in Gothic 2:

```
case WeaponState::Mage:
  return g2 ? Camera::Ranged : Camera::Melee;
```

The original always selects `CamModMagic` for spell casting. As a result OG plays
the `CAMMODRANGED` instance (bow/crossbow distances and rotation) while the player
has a spell readied, instead of the `CAMMODMAGIC` instance. The bow/crossbow case
(`Bow`/`CBow` -> `Camera::Ranged`) already matches the original.

## Proposed patch

OG symbols verified to exist: `Camera::Magic` (`game/camera.h:26`), routed to
`mageCam()` in `Camera::cameraDef` (`game/camera.cpp:545-546`).

The decompiled original is from the G2 addon and applies the magic mapping
unconditionally (no game-version branch in that block). The fix is therefore
scoped to the existing `g2` branch only, preserving the current Gothic 1 behavior
to stay surgical.

`game/mainwindow.cpp`, in `MainWindow::solveCameraMode`:

OLD:
```cpp
      case WeaponState::Mage:
        return g2 ? Camera::Ranged : Camera::Melee;
```

NEW:
```cpp
      case WeaponState::Mage:
        // NOTE: in original-game oCAIHuman::ChangeCamModeBySituation @0x0069cd00,
        // weapon mode 7 (spell) selects CamModMagic, not CamModRanged.
        return g2 ? Camera::Magic : Camera::Melee;
```
