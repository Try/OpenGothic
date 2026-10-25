#include "utils/gamepadsettings.h"

#include <gtest/gtest.h>
#include <set>

namespace {
using Settings = GamepadSettings;
using Button = Settings::Pad::Button;

TEST(GamepadSettings, DefaultsAndStableRoundtrip) {
  const Settings defaults;
  EXPECT_EQ(35, defaults.moveDeadZone);
  EXPECT_EQ(20, defaults.lookDeadZone);
  EXPECT_EQ(100, defaults.sensitivityX);
  EXPECT_EQ(100, defaults.sensitivityY);
  EXPECT_FALSE(defaults.invertY);
  EXPECT_TRUE(defaults.hints);
  EXPECT_EQ(defaults, Settings::parse(defaults.serialize()));
  auto changed = Settings::parse("v1 move=5 look=0 sx=25 sy=300 invert=1 hints=0 south=jump view=idle");
  EXPECT_EQ(5, changed.moveDeadZone);
  EXPECT_EQ(0, changed.lookDeadZone);
  EXPECT_EQ(25, changed.sensitivityX);
  EXPECT_EQ(300, changed.sensitivityY);
  EXPECT_TRUE(changed.invertY);
  EXPECT_FALSE(changed.hints);
  EXPECT_EQ(KeyCodec::Jump, changed.bindings[size_t(Button::South)]);
  EXPECT_EQ(KeyCodec::Idle, changed.bindings[size_t(Button::View)]);
  EXPECT_EQ(changed, Settings::parse(changed.serialize()));
  EXPECT_EQ(70, Settings::parse("v1 move=70 look=50 sx=300 sy=25").moveDeadZone);
  EXPECT_EQ(50, Settings::parse("v1 move=70 look=50 sx=300 sy=25").lookDeadZone);
  }

TEST(GamepadSettings, InvalidVersionMissingAndMalformedNumbersUseDefaults) {
  const Settings defaults;
  for(const auto text : {"", "v2 move=5", "v10 look=0", "move=5", "v1"})
    EXPECT_EQ(defaults, Settings::parse(text)) << text;
  for(const auto value : {"", "nan", "inf", "-1", "+35", "35.0", "35x", "99999999999999999999999"}) {
    const std::string text = "v1 move="+std::string(value)+" look="+value+" sx="+value+" sy="+value+" invert="+value+" hints="+value;
    EXPECT_EQ(defaults, Settings::parse(text)) << text;
    }
  EXPECT_EQ(defaults, Settings::parse("v1 move=4 look=51 sx=24 sy=301 invert=2 hints=-1"));
  EXPECT_EQ(defaults, Settings::parse("v1 move=71 look=-1 sx=301 sy=24"));
  EXPECT_EQ(defaults, Settings::parse("v1 nonsense unknown=5 south=unknown east=ui_settings west=k_f8 north=escape"));
  EXPECT_EQ(25, Settings::parse("v1 move=nan look=nan sx=25").sensitivityX);
  // A later malformed duplicate resets its own field, not the entire profile.
  const auto duplicate = Settings::parse("v1 sx=200 sx=no sy=250 south=jump south=unknown");
  EXPECT_EQ(100, duplicate.sensitivityX);
  EXPECT_EQ(250, duplicate.sensitivityY);
  EXPECT_EQ(defaults.bindings[size_t(Button::South)], duplicate.bindings[size_t(Button::South)]);
  }

TEST(GamepadSettings, WhitelistReservedButtonsAndNormalization) {
  const Settings defaults;
  auto settings = Settings::parse("v1 menu=jump home=action_generic south=escape");
  EXPECT_EQ(defaults, settings);
  settings.moveDeadZone = 100;
  settings.lookDeadZone = -1;
  settings.sensitivityX = 0;
  settings.sensitivityY = 1000;
  settings.bindings[size_t(Button::Menu)] = KeyCodec::Jump;
  settings.bindings[size_t(Button::Home)] = KeyCodec::Jump;
  for(auto button : Settings::remappableButtons)
    settings.bindings[size_t(button)] = KeyCodec::UiSettings;
  EXPECT_EQ(defaults.serialize(), settings.serialize());
  settings.normalize();
  EXPECT_EQ(defaults, settings);
  std::set<std::string_view> ids;
  for(auto button : Settings::remappableButtons) {
    EXPECT_NE(Button::Menu, button);
    EXPECT_NE(Button::Home, button);
    EXPECT_TRUE(ids.insert(Settings::buttonId(button)).second);
    EXPECT_FALSE(Settings::buttonName(button).empty());
    }
  ids.clear();
  for(auto action : Settings::allowedActions) {
    EXPECT_TRUE(action==KeyCodec::Idle || (action>=KeyCodec::Inventory && action<=KeyCodec::WeaponMageNext));
    EXPECT_TRUE(ids.insert(Settings::actionId(action)).second);
    EXPECT_FALSE(Settings::actionName(action).empty());
    }
  EXPECT_EQ("view", Settings::buttonId(Button::View));
  EXPECT_EQ("Create", Settings::buttonName(Button::View));
  EXPECT_TRUE(Settings::buttonId(Button::Count).empty());
  EXPECT_TRUE(Settings::actionId(KeyCodec::UiSettings).empty());
  }
}
