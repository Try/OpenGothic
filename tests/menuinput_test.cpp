#include "ui/menuinput.h"
#include "utils/gamepadinput.h"

#include <gtest/gtest.h>

namespace {
using Action = KeyCodec::Action;
using Input = GamepadInput;
using Pad = Tempest::Gamepad;

TEST(MenuInput, HorizontalCommandsCannotActivateOrSkipSaveConfirmation) {
  // TEXT commands and INPUT save/load slots share the command policy.
  for(auto direction : {KeyCodec::Left, KeyCodec::Right}) {
    EXPECT_EQ(KeyCodec::Idle, MenuInput::action(direction,MenuInput::Item::Command));
    EXPECT_EQ(direction, MenuInput::action(direction,MenuInput::Item::Value));
    }
  EXPECT_EQ(KeyCodec::Forward, MenuInput::action(KeyCodec::Left,MenuInput::Item::Horizontal));
  EXPECT_EQ(KeyCodec::Back, MenuInput::action(KeyCodec::Right,MenuInput::Item::Horizontal));
  for(auto item : {MenuInput::Item::Command, MenuInput::Item::Value, MenuInput::Item::Horizontal}) {
    EXPECT_EQ(KeyCodec::ActionGeneric, MenuInput::action(KeyCodec::ActionGeneric,item));
    EXPECT_EQ(KeyCodec::Forward, MenuInput::action(KeyCodec::Forward,item));
    EXPECT_EQ(KeyCodec::Back, MenuInput::action(KeyCodec::Back,item));
    }
  }

TEST(MenuInput, NestedJournalRoutesOnlyTopDialogAndRestoresParent) {
  uint64_t revision = 0;
  MenuModalInput modal(revision);
  int list = 0, content = 0;
  EXPECT_FALSE(modal.active());
  {
    MenuModalInput::Scope listScope(modal,[&](Action) { ++list; });
    EXPECT_EQ(1u, revision);
    EXPECT_TRUE(modal.active());
    EXPECT_FALSE(modal.blocked());
    modal.dispatch(KeyCodec::Forward);
    {
      MenuModalInput::Scope contentScope(modal,[&](Action) { ++content; });
      EXPECT_EQ(2u, revision);
      modal.dispatch(KeyCodec::Back);
      EXPECT_EQ(1, list);
      EXPECT_EQ(1, content);
      }
    EXPECT_EQ(3u, revision);
    modal.dispatch(KeyCodec::Escape);
    EXPECT_EQ(2, list);
    }
  EXPECT_EQ(4u, revision);
  EXPECT_FALSE(modal.active());
  EXPECT_FALSE(modal.blocked());
  modal.dispatch(KeyCodec::ActionGeneric);
  EXPECT_EQ(2, list);
  }

TEST(MenuInput, UnsupportedNestedModalBlocksRatherThanInvokingParent) {
  uint64_t revision = 0;
  MenuModalInput modal(revision);
  int count = 0;
  MenuModalInput::Scope parent(modal,[&](Action) { ++count; });
  {
    MenuModalInput::Scope unsupported(modal);
    EXPECT_TRUE(modal.blocked());
    modal.dispatch(KeyCodec::ActionGeneric);
    EXPECT_EQ(0, count);
    }
  EXPECT_FALSE(modal.blocked());
  modal.dispatch(KeyCodec::ActionGeneric);
  EXPECT_EQ(1, count);
  }

TEST(MenuInput, HandlerSurvivesReentrantReplacementUntilItReturns) {
  uint64_t revision = 0;
  MenuModalInput modal(revision);
  std::string trace;
  MenuModalInput::Scope parent(modal,[&](Action) {
    trace += 'L';
    {
      MenuModalInput::Scope nested(modal,[&](Action) { trace += 'C'; });
      modal.dispatch(KeyCodec::Forward);
      }
    trace += 'R';
    });
  modal.dispatch(KeyCodec::ActionGeneric);
  EXPECT_EQ("LCR", trace);
  modal.dispatch(KeyCodec::ActionGeneric);
  EXPECT_EQ("LCRLCR", trace);
  }

TEST(MenuInput, SaveConfirmationPreservesNameAndSuppliesDefaultOnlyForGamepad) {
  for(const auto* existing : {"My save", ""}) {
    std::string text = existing;
    MenuInput::SaveName decision(text);
    EXPECT_FALSE(decision.isAccepted());
    EXPECT_TRUE(decision.isPending());
    decision.accept(true);
    EXPECT_FALSE(decision.isPending());
    EXPECT_TRUE(decision.isAccepted());
    EXPECT_EQ(existing[0]=='\0' ? "Saved game" : existing, text);
    }
  std::string text;
  MenuInput::SaveName keyboard(text);
  keyboard.accept();
  EXPECT_TRUE(keyboard.isAccepted());
  EXPECT_TRUE(text.empty());
  }

TEST(MenuInput, CancelRestoresEditedNameAndUnusedSlotPlaceholder) {
  for(const auto* existing : {"My save", "---", ""}) {
    std::string text = existing;
    MenuInput::SaveName decision(text);
    text = "changed";
    decision.cancel();
    EXPECT_FALSE(decision.isPending());
    EXPECT_EQ(existing, text);
    EXPECT_FALSE(decision.isAccepted());
    }
  }

TEST(MenuInput, CancelCannotBeOverriddenByQueuedConfirmOrRepeatedCancel) {
  for(const auto* original : {"My save", "---", ""}) {
    for(bool padConfirm : {false,true}) {
      std::string text = original;
      MenuInput::SaveName decision(text);
      text.clear();
      decision.cancel();
      decision.accept(padConfirm);
      decision.cancel();
      decision.accept(padConfirm);
      EXPECT_FALSE(decision.isAccepted());
      EXPECT_EQ(original, text);
      }
    }
  }

TEST(MenuInput, AcceptCannotBeOverriddenByQueuedCancelOrRepeatedConfirm) {
  for(bool padConfirm : {false,true}) {
    std::string text = "Original name";
    MenuInput::SaveName decision(text);
    text.clear();
    decision.accept(padConfirm);
    const auto acceptedText = text;
    decision.cancel();
    decision.accept(true);
    decision.cancel();
    EXPECT_TRUE(decision.isAccepted());
    EXPECT_EQ(acceptedText, text);
    EXPECT_EQ(padConfirm ? "Saved game" : "", text);
    }
  }

TEST(MenuInput, EnteringAndLeavingModalRequiresFreshConfirmAndBackEdges) {
  using Button = Pad::Button;
  for(auto button : {Button::South, Button::East, Button::Menu}) {
    uint64_t revision = 0;
    MenuModalInput modal(revision);
    Input router;
    router.selectDevice(1,{});
    router.setContext({Input::Context::Menu,revision},{});
    Pad::State state;
    state.buttons = uint32_t(1)<<uint8_t(button);
    const Pad::Event down{Pad::Event::Type::ButtonDown,1,button};
    const Pad::Event up{Pad::Event::Type::ButtonUp,1,button};
    EXPECT_EQ(Input::Command::Type::Navigate, router.process(down,0).type);
    int count = 0;
    {
      MenuModalInput::Scope scope(modal,[&](Action) { ++count; });
      router.setContext({Input::Context::Menu,revision},state);
      EXPECT_EQ(Input::Command::Type::None, router.process(down,1).type);
      EXPECT_EQ(Input::Command::Type::None, router.update(state,1000).type);
      router.process(up,1001);
      auto command = router.process(down,1002);
      ASSERT_EQ(Input::Command::Type::Navigate, command.type);
      modal.dispatch(command.action);
      EXPECT_EQ(1, count);
      }
    router.setContext({Input::Context::Menu,revision},state);
    EXPECT_EQ(Input::Command::Type::None, router.update(state,2000).type);
    router.process(up,2001);
    EXPECT_EQ(Input::Command::Type::Navigate, router.process(down,2002).type);
    }
  }
}
