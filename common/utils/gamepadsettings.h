#pragma once

#include <Tempest/Gamepad>
#include <array>
#include <string>
#include <string_view>

#include "keycodec.h"

struct GamepadSettings final {
  using Pad = Tempest::Gamepad;

  int moveDeadZone = 35;
  int lookDeadZone = 20;
  int sensitivityX = 100;
  int sensitivityY = 100;
  bool invertY = false;
  bool hints = true;
  std::array<KeyCodec::Action, size_t(Pad::Button::Count)> bindings;

  GamepadSettings();
  static GamepadSettings parse(std::string_view profile);
  std::string serialize() const;
  void normalize();
  bool operator==(const GamepadSettings& other) const;

  static std::string_view buttonId(Pad::Button button);
  static std::string_view buttonName(Pad::Button button);
  static std::string_view actionId(KeyCodec::Action action);
  static std::string_view actionName(KeyCodec::Action action);

  static constexpr std::array<Pad::Button, 15> remappableButtons = {
    Pad::Button::South, Pad::Button::East, Pad::Button::West,
    Pad::Button::North, Pad::Button::DpadUp, Pad::Button::DpadDown,
    Pad::Button::DpadLeft, Pad::Button::DpadRight, Pad::Button::LeftShoulder,
    Pad::Button::RightShoulder, Pad::Button::LeftThumb, Pad::Button::RightThumb,
    Pad::Button::LeftTrigger, Pad::Button::RightTrigger, Pad::Button::View
    };
  static constexpr std::array<KeyCodec::Action, 35> allowedActions = {
    KeyCodec::Idle, KeyCodec::Inventory, KeyCodec::Status,
    KeyCodec::Log, KeyCodec::Map, KeyCodec::Heal,
    KeyCodec::Potion, KeyCodec::LookBack, KeyCodec::FirstPerson,
    KeyCodec::Forward, KeyCodec::Back, KeyCodec::Left,
    KeyCodec::Right, KeyCodec::RotateL, KeyCodec::RotateR,
    KeyCodec::Jump, KeyCodec::ActionGeneric, KeyCodec::ActionLeft,
    KeyCodec::ActionRight, KeyCodec::Parade, KeyCodec::Walk,
    KeyCodec::Sneak, KeyCodec::Weapon, KeyCodec::WeaponMele,
    KeyCodec::WeaponBow, KeyCodec::WeaponMage3, KeyCodec::WeaponMage4,
    KeyCodec::WeaponMage5, KeyCodec::WeaponMage6, KeyCodec::WeaponMage7,
    KeyCodec::WeaponMage8, KeyCodec::WeaponMage9, KeyCodec::WeaponMage10,
    KeyCodec::WeaponMagePrev, KeyCodec::WeaponMageNext
    };
  };
