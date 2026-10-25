#include "ui/gamepadinventoryinput.h"

#include <gtest/gtest.h>
#include <limits>

using Input = GamepadInventoryInput;

TEST(GamepadInventoryInput, QuantityBoundsAndStepsDoNotOverflow) {
  Input input;
  Input::Selection selected;
  EXPECT_FALSE(input.beginQuantity(selected));
  for(auto count : {size_t(1),size_t(9),size_t(10),size_t(11),std::numeric_limits<size_t>::max()}) {
    selected.count = count;
    ASSERT_TRUE(input.beginQuantity(selected));
    EXPECT_EQ(input.quantity(),1u);
    input.process(KeyCodec::Left);
    input.process(KeyCodec::Back);
    EXPECT_EQ(input.quantity(),1u);
    input.process(KeyCodec::Forward);
    EXPECT_EQ(input.quantity(),std::min(count,size_t(11)));
    input.process(KeyCodec::UiNext);
    EXPECT_EQ(input.quantity(),count);
    input.process(KeyCodec::Forward);
    input.process(KeyCodec::Right);
    EXPECT_EQ(input.quantity(),count);
    input.process(KeyCodec::UiPrevious);
    input.process(KeyCodec::Right);
    EXPECT_EQ(input.quantity(),std::min(count,size_t(2)));
    }
  }

TEST(GamepadInventoryInput, SlotSelectionWrapsAndNeverLeavesThreeToTen) {
  Input input;
  Input::Selection selected;
  for(int initial=0; initial<=255; ++initial) {
    selected.slot = uint8_t(initial);
    input.beginSpell(selected);
    EXPECT_EQ(input.slot(),initial>=3 && initial<=10 ? initial : 3);
    for(int i=0; i<16; ++i) {
      input.process(KeyCodec::UiPrevious);
      EXPECT_GE(input.slot(),3);
      EXPECT_LE(input.slot(),10);
      }
    }
  selected.slot = 3;
  input.beginSpell(selected);
  input.process(KeyCodec::Left);
  EXPECT_EQ(input.slot(),10);
  input.process(KeyCodec::Right);
  EXPECT_EQ(input.slot(),3);
  input.process(KeyCodec::Forward);
  EXPECT_EQ(input.slot(),10);
  input.process(KeyCodec::Back);
  EXPECT_EQ(input.slot(),3);
  }

TEST(GamepadInventoryInput, EverySelectionComponentInvalidatesPendingOperation) {
  Input input;
  Input::Selection selected = {1,0,4,52,100,255,false};
  ASSERT_TRUE(input.beginQuantity(selected));
  EXPECT_TRUE(input.matches(selected));
  for(int field=0; field<7; ++field) {
    auto changed = selected;
    switch(field) {
      case 0: ++changed.state; break;
      case 1: ++changed.panel; break;
      case 2: ++changed.index; break;
      case 3: ++changed.cls; break;
      case 4: --changed.count; break;
      case 5: changed.slot = 3; break;
      case 6: changed.equipped = true; break;
      }
    EXPECT_FALSE(input.matches(changed));
    }
  }

TEST(GamepadInventoryInput, OnlyExplicitConfirmRequestsCommitAndResetDiscardsSelection) {
  Input input;
  Input::Selection selected;
  selected.count = 99;
  EXPECT_EQ(input.process(KeyCodec::ActionGeneric),Input::Result::None);
  input.beginQuantity(selected);
  EXPECT_EQ(input.process(KeyCodec::Forward),Input::Result::Changed);
  EXPECT_EQ(input.process(KeyCodec::UiAmount),Input::Result::None);
  EXPECT_EQ(input.process(KeyCodec::Escape),Input::Result::Cancel);
  input.reset();
  EXPECT_EQ(input.mode(),Input::Mode::Browse);
  EXPECT_FALSE(input.matches(selected));
  EXPECT_EQ(input.process(KeyCodec::ActionGeneric),Input::Result::None);
  input.beginSpell(selected);
  EXPECT_EQ(input.process(KeyCodec::ActionGeneric),Input::Result::Confirm);
  input.reset();
  input.beginQuantity(selected);
  EXPECT_EQ(input.quantity(),1u);
  }
