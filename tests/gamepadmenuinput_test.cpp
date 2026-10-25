#include "ui/gamepadmenuinput.h"

#include <gtest/gtest.h>

namespace {
using Command = GamepadInput::Command;
using Context = GamepadInput::Context;
using Screen = GamepadInput::Screen;
using Pad = Tempest::Gamepad;

constexpr Screen menu{Context::Menu,1};
constexpr Command confirm{Command::Type::Navigate,KeyCodec::ActionGeneric};
constexpr Command back{Command::Type::Navigate,KeyCodec::Escape};

TEST(GamepadMenuInput, OnlyMenuNavigationFromADeviceCanBeDeferred) {
  GamepadMenuInput input;
  for(auto type : {Command::Type::None,Command::Type::Press,Command::Type::Release,
                   Command::Type::Cancel,Command::Type::CancelAll})
    input.push({type,KeyCodec::ActionGeneric},menu,1);
  input.push(confirm,{Context::Inventory,1},1);
  input.push(confirm,menu,0);
  EXPECT_TRUE(input.empty());
  input.push(confirm,menu,1);
  EXPECT_EQ(KeyCodec::ActionGeneric,input.pop(menu,1).action);
  EXPECT_TRUE(input.empty());
  }

TEST(GamepadMenuInput, PreservesOrderAndConsumesEachCommandOnce) {
  GamepadMenuInput input;
  input.push({Command::Type::Navigate,KeyCodec::Back},menu,1);
  input.push(confirm,menu,1);
  input.push(back,menu,1);
  EXPECT_EQ(KeyCodec::Back,input.pop(menu,1).action);
  EXPECT_EQ(KeyCodec::ActionGeneric,input.pop(menu,1).action);
  EXPECT_EQ(KeyCodec::Escape,input.pop(menu,1).action);
  EXPECT_EQ(Command::Type::None,input.pop(menu,1).type);
  }

TEST(GamepadMenuInput, ContextRevisionAndDeviceEachInvalidateQueuedActions) {
  for(auto screen : {Screen{Context::Gameplay,1},Screen{Context::Menu,2},
                     Screen{Context::Blocked,1}}) {
    GamepadMenuInput input;
    input.push(confirm,menu,1);
    EXPECT_EQ(Command::Type::None,input.pop(screen,1).type);
    EXPECT_TRUE(input.empty());
    }
  GamepadMenuInput input;
  input.push(confirm,menu,1);
  input.push(back,menu,2);
  EXPECT_EQ(KeyCodec::Escape,input.pop(menu,2).action);
  EXPECT_TRUE(input.empty());
  }

TEST(GamepadMenuInput, ParentCommandIsRemovedBeforeNestedModalDispatch) {
  GamepadMenuInput input;
  input.push(confirm,menu,1);
  input.push(confirm,menu,1);
  EXPECT_EQ(KeyCodec::ActionGeneric,input.pop(menu,1).action);
  const Screen child{Context::Menu,2};
  input.push(back,child,1);
  EXPECT_EQ(KeyCodec::Escape,input.pop(child,1).action);
  EXPECT_TRUE(input.empty());
  // Returning to the parent cannot replay its already consumed command.
  EXPECT_EQ(Command::Type::None,input.pop({Context::Menu,3},1).type);
  }

TEST(GamepadMenuInput, CancellationDropsPendingEvenWhenScreenAndDeviceStayTheSame) {
  GamepadMenuInput input;
  input.push(confirm,menu,1);
  input.push(back,menu,1);
  input.clear();
  EXPECT_EQ(Command::Type::None,input.pop(menu,1).type);
  input.push(back,menu,1);
  EXPECT_EQ(KeyCodec::Escape,input.pop(menu,1).action);
  }

TEST(GamepadMenuInput, OverflowDropsEveryCommandAndRequiresRouterNeutralRearm) {
  GamepadMenuInput input;
  GamepadInput router;
  router.selectDevice(1,{});
  router.setContext(menu,{});
  for(size_t i=0;i<GamepadMenuInput::MaxCommands;++i)
    input.push(confirm,menu,1);
  EXPECT_FALSE(input.overflowed());
  input.push(back,menu,1);
  EXPECT_TRUE(input.overflowed());
  EXPECT_TRUE(input.empty());
  input.push(confirm,menu,1);
  EXPECT_EQ(Command::Type::None,input.pop(menu,1).type);

  Pad::State held;
  held.buttons = uint32_t(1)<<uint8_t(Pad::Button::South);
  held.axes[size_t(Pad::Axis::LeftY)] = 1.f;
  // Boundary recovery uses the final backend snapshot after draining events.
  input.clear();
  router.reset(held);
  input.push(router.update(held,1000),menu,1);
  input.push(router.process({Pad::Event::Type::ButtonDown,1,Pad::Button::South},1001),menu,1);
  EXPECT_FALSE(input.overflowed());
  EXPECT_TRUE(input.empty());
  router.process({Pad::Event::Type::ButtonUp,1,Pad::Button::South},1002);
  EXPECT_EQ(Command::Type::None,router.update({},1003).type);
  input.push(router.process({Pad::Event::Type::ButtonDown,1,Pad::Button::South},1004),menu,1);
  EXPECT_EQ(KeyCodec::ActionGeneric,input.pop(menu,1).action);
  }
}
