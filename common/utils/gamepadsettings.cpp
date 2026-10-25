#include "gamepadsettings.h"

#include <algorithm>
#include <charconv>

namespace {
struct ActionInfo {
  KeyCodec::Action action;
  std::string_view id;
  std::string_view name;
  };
constexpr ActionInfo actions[] = {
  {KeyCodec::Idle, "idle", "Idle"},
  {KeyCodec::Inventory, "inventory", "Inventory"},
  {KeyCodec::Status, "status", "Status"},
  {KeyCodec::Log, "log", "Log"},
  {KeyCodec::Map, "map", "Map"},
  {KeyCodec::Heal, "heal", "Heal"},
  {KeyCodec::Potion, "potion", "Potion"},
  {KeyCodec::LookBack, "look_back", "Look Back"},
  {KeyCodec::FirstPerson, "first_person", "First Person"},
  {KeyCodec::Forward, "forward", "Forward"},
  {KeyCodec::Back, "back", "Back"},
  {KeyCodec::Left, "left", "Left"},
  {KeyCodec::Right, "right", "Right"},
  {KeyCodec::RotateL, "rotate_l", "Turn left"},
  {KeyCodec::RotateR, "rotate_r", "Turn right"},
  {KeyCodec::Jump, "jump", "Jump"},
  {KeyCodec::ActionGeneric, "action_generic", "Action / attack"},
  {KeyCodec::ActionLeft, "action_left", "Left attack"},
  {KeyCodec::ActionRight, "action_right", "Right attack"},
  {KeyCodec::Parade, "parade", "Block"},
  {KeyCodec::Walk, "walk", "Walk"},
  {KeyCodec::Sneak, "sneak", "Sneak"},
  {KeyCodec::Weapon, "weapon", "Weapon"},
  {KeyCodec::WeaponMele, "weapon_mele", "Melee weapon"},
  {KeyCodec::WeaponBow, "weapon_bow", "Ranged weapon"},
  {KeyCodec::WeaponMage3, "weapon_mage3", "Spell 3"},
  {KeyCodec::WeaponMage4, "weapon_mage4", "Spell 4"},
  {KeyCodec::WeaponMage5, "weapon_mage5", "Spell 5"},
  {KeyCodec::WeaponMage6, "weapon_mage6", "Spell 6"},
  {KeyCodec::WeaponMage7, "weapon_mage7", "Spell 7"},
  {KeyCodec::WeaponMage8, "weapon_mage8", "Spell 8"},
  {KeyCodec::WeaponMage9, "weapon_mage9", "Spell 9"},
  {KeyCodec::WeaponMage10, "weapon_mage10", "Spell 10"},
  {KeyCodec::WeaponMagePrev, "weapon_mage_prev", "Previous spell"},
  {KeyCodec::WeaponMageNext, "weapon_mage_next", "Next spell"},
  {KeyCodec::Escape, "escape", "Back"}
  };
struct ButtonInfo { std::string_view id, name; };
constexpr ButtonInfo buttons[] = {
  {"south", "Cross"},
  {"east", "Circle"},
  {"west", "Square"},
  {"north", "Triangle"},
  {"dpad_up", "Dpad Up"},
  {"dpad_down", "Dpad Down"},
  {"dpad_left", "Dpad Left"},
  {"dpad_right", "Dpad Right"},
  {"left_shoulder", "L1"},
  {"right_shoulder", "R1"},
  {"left_thumb", "L3"},
  {"right_thumb", "R3"},
  {"left_trigger", "L2"},
  {"right_trigger", "R2"},
  {"menu", "Options"},
  {"view", "Create"},
  {"home", "PS"},
  };

std::string_view token(std::string_view& profile) {
  const auto start = profile.find_first_not_of(' ');
  if(start==std::string_view::npos) {
    profile = {};
    return {};
    }
  profile.remove_prefix(start);
  const auto end = profile.find(' ');
  const auto result = profile.substr(0, end);
  profile.remove_prefix(end==std::string_view::npos ? profile.size() : end);
  return result;
  }

int integer(std::string_view text, int low, int high, int fallback) {
  int value = 0;
  const auto result = std::from_chars(text.data(), text.data()+text.size(), value);
  return result.ec==std::errc() && result.ptr==text.data()+text.size() && value>=low && value<=high ? value : fallback;
  }

void range(int& value, int low, int high, int fallback) {
  if(value<low || value>high)
    value = fallback;
  }
}

GamepadSettings::GamepadSettings() : bindings{
  KeyCodec::ActionGeneric, KeyCodec::Jump, KeyCodec::Weapon,
  KeyCodec::Inventory, KeyCodec::WeaponMele, KeyCodec::WeaponBow,
  KeyCodec::WeaponMagePrev, KeyCodec::WeaponMageNext, KeyCodec::ActionLeft,
  KeyCodec::ActionRight, KeyCodec::Walk, KeyCodec::Sneak,
  KeyCodec::Parade, KeyCodec::ActionGeneric, KeyCodec::Escape,
  KeyCodec::Log, KeyCodec::Idle
  } {}

void GamepadSettings::normalize() {
  range(moveDeadZone, 5, 70, 35);
  range(lookDeadZone, 0, 50, 20);
  range(sensitivityX, 25, 300, 100);
  range(sensitivityY, 25, 300, 100);
  const GamepadSettings defaults;
  for(auto button : remappableButtons) {
    auto& action = bindings[size_t(button)];
    if(std::find(allowedActions.begin(), allowedActions.end(), action)==allowedActions.end())
      action = defaults.bindings[size_t(button)];
    }
  bindings[size_t(Pad::Button::Menu)] = KeyCodec::Escape;
  bindings[size_t(Pad::Button::Home)] = KeyCodec::Idle;
  }

GamepadSettings GamepadSettings::parse(std::string_view profile) {
  GamepadSettings result;
  if(token(profile)!="v1")
    return result;
  const GamepadSettings defaults;
  while(!profile.empty()) {
    const auto field = token(profile);
    const auto equal = field.find('=');
    if(equal==std::string_view::npos)
      continue;
    const auto key = field.substr(0, equal);
    const auto value = field.substr(equal+1);
    if(key=="move") result.moveDeadZone = integer(value, 5, 70, 35);
    else if(key=="look") result.lookDeadZone = integer(value, 0, 50, 20);
    else if(key=="sx") result.sensitivityX = integer(value, 25, 300, 100);
    else if(key=="sy") result.sensitivityY = integer(value, 25, 300, 100);
    else if(key=="invert") result.invertY = integer(value, 0, 1, 0)!=0;
    else if(key=="hints") result.hints = integer(value, 0, 1, 1)!=0;
    else for(auto button : remappableButtons) {
      if(key!=buttonId(button))
        continue;
      auto& binding = result.bindings[size_t(button)];
      binding = defaults.bindings[size_t(button)];
      for(auto action : allowedActions)
        if(value==actionId(action)) {
          binding = action;
          break;
          }
      break;
      }
    }
  return result;
  }

std::string GamepadSettings::serialize() const {
  auto profile = *this;
  profile.normalize();
  std::string result = "v1 move="+std::to_string(profile.moveDeadZone)+
      " look="+std::to_string(profile.lookDeadZone)+" sx="+std::to_string(profile.sensitivityX)+
      " sy="+std::to_string(profile.sensitivityY)+" invert="+(profile.invertY ? "1" : "0")+
      " hints="+(profile.hints ? "1" : "0");
  for(auto button : remappableButtons) {
    result += ' ';
    result += buttonId(button);
    result += '=';
    result += actionId(profile.bindings[size_t(button)]);
    }
  return result;
  }

bool GamepadSettings::operator==(const GamepadSettings& other) const {
  return moveDeadZone==other.moveDeadZone && lookDeadZone==other.lookDeadZone &&
         sensitivityX==other.sensitivityX && sensitivityY==other.sensitivityY &&
         invertY==other.invertY && hints==other.hints && bindings==other.bindings;
  }

std::string_view GamepadSettings::buttonId(Pad::Button button) {
  return button<Pad::Button::Count ? buttons[size_t(button)].id : std::string_view{};
  }

std::string_view GamepadSettings::buttonName(Pad::Button button) {
  return button<Pad::Button::Count ? buttons[size_t(button)].name : std::string_view{};
  }

std::string_view GamepadSettings::actionId(KeyCodec::Action action) {
  for(const auto& info : actions)
    if(info.action==action)
      return info.id;
  return {};
  }

std::string_view GamepadSettings::actionName(KeyCodec::Action action) {
  for(const auto& info : actions)
    if(info.action==action)
      return info.name;
  return {};
  }
