#include "ui/gamepadtextinput.h"

#include <gtest/gtest.h>

#include <set>

namespace {
void select(GamepadTextInput& input, size_t cell) {
  for(size_t i=0;i<GamepadTextInput::Cells && input.selection()!=cell;++i)
    input.move(KeyCodec::Right);
  ASSERT_EQ(cell,input.selection());
  }

TEST(GamepadTextInput, DefaultSaveAndEveryEnabledKeyIsReachable) {
  for(bool russian : {false,true}) {
    GamepadTextInput input(russian);
    EXPECT_EQ(GamepadTextInput::Save,input.selection());
    std::string name;
    EXPECT_EQ(GamepadTextInput::Result::Accept,input.activate(name));
    std::set<size_t> visited;
    for(size_t i=0;i<GamepadTextInput::Cells;++i) {
      visited.insert(input.selection());
      input.move(KeyCodec::Right);
      }
    for(size_t i=0;i<GamepadTextInput::Cells;++i)
      EXPECT_EQ(input.enabled(i),visited.count(i)!=0) << i;
    for(auto action : {KeyCodec::Left,KeyCodec::Right,KeyCodec::Forward,KeyCodec::Back})
      for(size_t i=0;i<GamepadTextInput::Cells;++i) {
        input.move(action);
        EXPECT_TRUE(input.enabled(input.selection()));
        }
    }
  }

TEST(GamepadTextInput, WritesBothAlphabetsAndBothCasesAsSingleByteCodepages) {
  GamepadTextInput input;
  std::string text;
  select(input,0);
  input.activate(text);
  select(input,GamepadTextInput::Case);
  input.activate(text);
  select(input,25);
  input.activate(text);
  select(input,GamepadTextInput::Language);
  input.activate(text);
  select(input,6);
  input.activate(text);
  select(input,GamepadTextInput::Case);
  input.activate(text);
  select(input,6);
  input.activate(text);
  EXPECT_EQ(std::string("aZ\xa8\xb8"),text);
  text.clear();
  for(size_t i=0;i<33;++i) {
    select(input,i);
    input.activate(text);
    }
  EXPECT_EQ(std::string("\xe0\xe1\xe2\xe3\xe4\xe5\xb8\xe6\xe7\xe8\xe9\xea\xeb\xec\xed\xee\xef\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff"),text);
  select(input,GamepadTextInput::Case);
  input.activate(text);
  text.clear();
  for(size_t i=0;i<33;++i) {
    select(input,i);
    input.activate(text);
    }
  EXPECT_EQ(std::string("\xc0\xc1\xc2\xc3\xc4\xc5\xa8\xc6\xc7\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9\xda\xdb\xdc\xdd\xde\xdf"),text);
  }

TEST(GamepadTextInput, DigitsSpaceDotEraseAndTerminalCommands) {
  GamepadTextInput input;
  std::string text;
  for(size_t i=40;i<=51;++i) {
    select(input,i);
    input.activate(text);
    }
  EXPECT_EQ("0123456789. ",text);
  select(input,GamepadTextInput::Erase);
  input.activate(text);
  EXPECT_EQ("0123456789.",text);
  text.clear();
  input.activate(text);
  EXPECT_TRUE(text.empty());
  select(input,GamepadTextInput::Cancel);
  EXPECT_EQ(GamepadTextInput::Result::Cancel,input.activate(text));
  }

TEST(GamepadTextInput, AppendIsBoundedWithoutTruncatingExistingLongNames) {
  GamepadTextInput input;
  select(input,0);
  std::string text(63,'x');
  input.activate(text);
  EXPECT_EQ(std::string(63,'x')+"a",text);
  input.activate(text);
  EXPECT_EQ(64u,text.size());
  text.assign(80,'x');
  input.activate(text);
  EXPECT_EQ(std::string(80,'x'),text);
  select(input,GamepadTextInput::Erase);
  input.activate(text);
  EXPECT_EQ(std::string(79,'x'),text);
  }
}
