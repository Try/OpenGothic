#include "ui/gamepadsettingsinput.h"

#include <gtest/gtest.h>

#include <limits>

using Input = GamepadSettingsInput;

TEST(GamepadSettingsEditor, DraftCancelAndLateApplyDoNotCommit) {
  GamepadSettings original;
  original.sensitivityX = 150;
  Input input(original);
  input.select(2);
  input.process(KeyCodec::Right);
  EXPECT_EQ(input.draft.sensitivityX,155);
  EXPECT_EQ(original.sensitivityX,150);
  input.process(KeyCodec::Escape);
  input.select(Input::Apply);
  input.process(KeyCodec::ActionGeneric);
  input.process(KeyCodec::Right);
  EXPECT_TRUE(input.finished);
  EXPECT_FALSE(input.accepted);
  EXPECT_EQ(input.draft.sensitivityX,155);
  }

TEST(GamepadSettingsEditor, ResetIsOnlyDraftAndFirstTerminalDecisionWins) {
  GamepadSettings original;
  original.invertY = true;
  original.bindings[size_t(GamepadSettings::Pad::Button::South)] = KeyCodec::Map;
  Input input(original);
  input.select(Input::Reset);
  input.process(KeyCodec::ActionGeneric);
  EXPECT_EQ(input.draft,GamepadSettings());
  EXPECT_FALSE(input.finished);
  EXPECT_TRUE(original.invertY);
  input.select(Input::Apply);
  input.process(KeyCodec::ActionGeneric);
  const auto accepted = input.draft;
  input.process(KeyCodec::Escape);
  input.select(Input::Reset);
  input.process(KeyCodec::ActionGeneric);
  EXPECT_TRUE(input.accepted);
  EXPECT_EQ(input.draft,accepted);
  }

TEST(GamepadSettingsEditor, EditsRemainInBoundsAndReservedButtonsCannotBeSelected) {
  Input input{GamepadSettings()};
  input.select(std::numeric_limits<int>::min());
  EXPECT_EQ(input.selected,0);
  for(int i=0;i<400;++i)
    input.process(KeyCodec::Left);
  EXPECT_EQ(input.draft.moveDeadZone,5);
  for(int row=0;row<4;++row) {
    input.select(row);
    for(int i=0;i<400;++i)
      input.process(KeyCodec::Right);
    }
  EXPECT_EQ(input.draft.moveDeadZone,70);
  EXPECT_EQ(input.draft.lookDeadZone,50);
  EXPECT_EQ(input.draft.sensitivityX,300);
  EXPECT_EQ(input.draft.sensitivityY,300);
  for(int row=Input::FirstBinding;row<Input::Apply;++row) {
    input.select(row);
    for(size_t i=0;i<GamepadSettings::allowedActions.size()+1;++i)
      input.process(KeyCodec::Right);
    }
  EXPECT_EQ(input.draft.bindings[size_t(GamepadSettings::Pad::Button::Menu)],KeyCodec::Escape);
  EXPECT_EQ(input.draft.bindings[size_t(GamepadSettings::Pad::Button::Home)],KeyCodec::Idle);
  input.select(std::numeric_limits<int>::max());
  EXPECT_EQ(input.selected,Input::Cancel);
  input.process(KeyCodec::Right);
  EXPECT_FALSE(input.finished);
  }
