// Compile this fixture with test-only -fno-access-control. Including the real
// implementation exposes its private dialog classes without production hooks.
#include "ui/gamemenu.cpp"
#include "utils/gamepadinput.h"

#include <Tempest/Application>
#include <gtest/gtest.h>

#include <functional>

namespace {
using Pad = Tempest::Gamepad;
using Button = Pad::Button;
using Command = GamepadInput::Command;

// Dialog::exec creates its actual overlay before invoking showEvent. Execute
// a bounded queue here so close->remaining edges occurs before scope unwind,
// without creating a renderer, loading assets or entering an unbounded loop.
template<class Base>
class QueuedDialog final : public Base {
  public:
    using Base::Base;
    std::function<void()> queue;
  protected:
    void showEvent() override {
      EXPECT_TRUE(this->isOpen());
      if(queue)
        queue();
      this->close();
      }
  };

class DialogQueue final {
  public:
    DialogQueue():modal(revision) {
      router.selectDevice(1,{});
      sync();
      }

    void sync() {
      router.setContext({GamepadInput::Context::Menu,revision},state);
      }

    Command edge(Button button, bool down) {
      const auto mask = uint32_t(1)<<uint8_t(button);
      if(down)
        state.buttons |= mask;
      else
        state.buttons &= ~mask;
      const Pad::Event event{down ? Pad::Event::Type::ButtonDown : Pad::Event::Type::ButtonUp,1,button};
      auto command = router.process(event,++time);
      if(command.type==Command::Type::Navigate)
        modal.dispatch(command.action);
      return command;
      }

    uint64_t revision = 0;
    MenuModalInput modal;
    GamepadInput router;
    Pad::State state;
    uint64_t time = 0;
  };

TEST(GamepadDialog, CancelThenQueuedConfirmCannotSaveBeforeScopeUnwinds) {
  for(const auto* original : {"Existing save", "", "---"}) {
    DialogQueue input;
    std::string name = original;
    QueuedDialog<GameMenu::SavNameDialog> dialog(name);
    // Match execSingle's empty-slot initialization after capturing text0.
    if(name=="---")
      name.clear();
    EXPECT_EQ(Command::Type::Navigate,input.edge(Button::South,true).type);
    {
      MenuModalInput::Scope scope(input.modal,[&](KeyCodec::Action action) { dialog.onGamepad(action); });
      input.sync();
      dialog.queue = [&] {
        input.edge(Button::South,false);
        EXPECT_EQ(Command::Type::Navigate,input.edge(Button::East,true).type);
        EXPECT_FALSE(dialog.isOpen());
        EXPECT_FALSE(dialog.decision.isAccepted());
        EXPECT_EQ(original,name);
        // A distinct queued confirm reaches the still-installed modal handler.
        EXPECT_EQ(Command::Type::Navigate,input.edge(Button::South,true).type);
        EXPECT_FALSE(dialog.decision.isAccepted());
        EXPECT_EQ(original,name);
        EXPECT_EQ(1u,input.revision);
        };
      dialog.exec();
      EXPECT_EQ(1u,input.revision);
      EXPECT_TRUE(input.modal.active());
      }
    EXPECT_EQ(2u,input.revision);
    input.sync();
    EXPECT_EQ(Command::Type::None,input.router.update(input.state,1000).type);
    EXPECT_EQ(Command::Type::None,input.edge(Button::South,true).type);
    input.edge(Button::South,false);
    EXPECT_EQ(Command::Type::Navigate,input.edge(Button::South,true).type);
    EXPECT_FALSE(dialog.decision.isAccepted());
    EXPECT_EQ(original,name);
    }
  }

TEST(GamepadDialog, AcceptThenQueuedCancelKeepsAcceptedNameAndRequiresFreshConfirm) {
  for(const auto* original : {"Existing save", "", "---"}) {
    DialogQueue input;
    std::string name = original;
    QueuedDialog<GameMenu::SavNameDialog> dialog(name);
    if(name=="---")
      name.clear();
    const std::string expected = name.empty() ? "Saved game" : name;
    input.edge(Button::South,true);
    {
      MenuModalInput::Scope scope(input.modal,[&](KeyCodec::Action action) { dialog.onGamepad(action); });
      input.sync();
      dialog.queue = [&] {
        // The button that opened the dialog must first be released.
        EXPECT_EQ(Command::Type::None,input.edge(Button::South,true).type);
        EXPECT_TRUE(dialog.isOpen());
        EXPECT_FALSE(dialog.decision.isAccepted());
        input.edge(Button::South,false);
        EXPECT_EQ(Command::Type::Navigate,input.edge(Button::South,true).type);
        EXPECT_FALSE(dialog.isOpen());
        EXPECT_TRUE(dialog.decision.isAccepted());
        EXPECT_EQ(expected,name);
        EXPECT_EQ(Command::Type::Navigate,input.edge(Button::East,true).type);
        EXPECT_TRUE(dialog.decision.isAccepted());
        EXPECT_EQ(expected,name);
        EXPECT_EQ(1u,input.revision);
        };
      dialog.exec();
      EXPECT_EQ(1u,input.revision);
      }
    EXPECT_EQ(2u,input.revision);
    input.sync();
    EXPECT_EQ(Command::Type::None,input.router.update(input.state,1000).type);
    EXPECT_EQ(Command::Type::None,input.edge(Button::South,true).type);
    input.edge(Button::South,false);
    EXPECT_EQ(Command::Type::Navigate,input.edge(Button::South,true).type);
    EXPECT_EQ(expected,name);
    }
  }

void selectCell(GameMenu::SavNameDialog& dialog, size_t cell) {
  for(size_t i=0;i<GamepadTextInput::Cells && dialog.input.selection()!=cell;++i)
    dialog.onGamepad(KeyCodec::Right);
  ASSERT_EQ(cell,dialog.input.selection());
  }

TEST(GamepadDialog, EverySourceIgnoresLateEditsAfterEitherTerminalDecision) {
  for(bool accept : {false,true}) {
    for(const auto* original : {"Original", "", "---"}) {
      std::string name = original;
      QueuedDialog<GameMenu::SavNameDialog> dialog(name);
      if(name=="---")
        name.clear();
      dialog.queue = [&] {
        Tempest::KeyEvent character(uint32_t('x'));
        dialog.keyUpEvent(character);
        const auto edited = name;
        dialog.onGamepad(accept ? KeyCodec::ActionGeneric : KeyCodec::Escape);
        EXPECT_FALSE(dialog.isOpen());
        const auto expected = accept ? edited : std::string(original);
        for(int repeat=0;repeat<2;++repeat) {
          Tempest::KeyEvent back(Tempest::Event::K_Back);
          Tempest::KeyEvent enter(Tempest::Event::K_Return);
          Tempest::KeyEvent escape(Tempest::Event::K_ESCAPE);
          Tempest::MouseEvent click(1,1,Tempest::Event::ButtonLeft);
          dialog.keyUpEvent(back);
          dialog.keyUpEvent(character);
          dialog.keyUpEvent(enter);
          dialog.keyUpEvent(escape);
          dialog.cells[GamepadTextInput::Save] = Tempest::Rect(0,0,10,10);
          dialog.pressedCell = GamepadTextInput::Save;
          dialog.mouseUpEvent(click);
          dialog.mouseDownEvent(click);
          dialog.onGamepad(KeyCodec::Left);
          dialog.onGamepad(KeyCodec::ActionGeneric);
          dialog.onGamepad(KeyCodec::Escape);
          EXPECT_EQ(expected,name);
          EXPECT_EQ(accept,dialog.decision.isAccepted());
          }
        };
      dialog.exec();
      }
    }
  }

TEST(GamepadDialog, KeyboardAndMouseTerminalPathsFreezeAllLaterSources) {
  for(bool mouse : {false,true}) {
    for(bool accept : {false,true}) {
      std::string name = "Original";
      QueuedDialog<GameMenu::SavNameDialog> dialog(name);
      name = "Edited";
      dialog.queue = [&] {
        if(mouse) {
          const auto cell = accept ? GamepadTextInput::Save : GamepadTextInput::Cancel;
          dialog.cells[cell] = Tempest::Rect(0,0,10,10);
          Tempest::MouseEvent click(1,1,Tempest::Event::ButtonLeft);
          dialog.mouseDownEvent(click);
          dialog.mouseUpEvent(click);
          }
        else {
          Tempest::KeyEvent key(accept ? Tempest::Event::K_Return : Tempest::Event::K_ESCAPE);
          dialog.keyUpEvent(key);
          }
        EXPECT_FALSE(dialog.isOpen());
        Tempest::KeyEvent character(uint32_t('x'));
        Tempest::KeyEvent back(Tempest::Event::K_Back);
        dialog.keyUpEvent(character);
        dialog.keyUpEvent(back);
        dialog.onGamepad(KeyCodec::ActionGeneric);
        dialog.onGamepad(KeyCodec::Escape);
        EXPECT_EQ(accept ? "Edited" : "Original",name);
        EXPECT_EQ(accept,dialog.decision.isAccepted());
        };
      dialog.exec();
      }
    }
  }

TEST(GamepadDialog, EmptyKeyboardAndMouseAcceptDoNotSupplyGamepadDefault) {
  for(bool mouse : {false,true}) {
    std::string name;
    QueuedDialog<GameMenu::SavNameDialog> dialog(name);
    dialog.queue = [&] {
      if(mouse) {
        dialog.cells[GamepadTextInput::Save] = Tempest::Rect(0,0,10,10);
        Tempest::MouseEvent click(1,1,Tempest::Event::ButtonLeft);
        dialog.mouseDownEvent(click);
        dialog.mouseUpEvent(click);
        }
      else {
        Tempest::KeyEvent key(Tempest::Event::K_Return);
        dialog.keyUpEvent(key);
        }
      EXPECT_TRUE(dialog.decision.isAccepted());
      EXPECT_TRUE(name.empty());
      };
    dialog.exec();
    }
  }

TEST(GamepadDialog, TextEntryUsesExactGameCodepageBytesAndPreservesKeyboardContract) {
  std::string name;
  QueuedDialog<GameMenu::SavNameDialog> dialog(name);
  dialog.queue = [&] {
    selectCell(dialog,0);
    dialog.onGamepad(KeyCodec::ActionGeneric);
    selectCell(dialog,GamepadTextInput::Language);
    dialog.onGamepad(KeyCodec::ActionGeneric);
    selectCell(dialog,6);
    dialog.onGamepad(KeyCodec::ActionGeneric);
    selectCell(dialog,GamepadTextInput::Case);
    dialog.onGamepad(KeyCodec::ActionGeneric);
    selectCell(dialog,6);
    dialog.onGamepad(KeyCodec::ActionGeneric);
    EXPECT_EQ(std::string("a\xb8\xa8"),name);
    name.assign(64,'x');
    dialog.onGamepad(KeyCodec::ActionGeneric);
    EXPECT_EQ(64u,name.size());
    Tempest::KeyEvent character(uint32_t('a'));
    dialog.keyUpEvent(character);
    EXPECT_EQ(std::string(64,'x')+"a",name);
    selectCell(dialog,GamepadTextInput::Save);
    dialog.onGamepad(KeyCodec::ActionGeneric);
    EXPECT_TRUE(dialog.decision.isAccepted());
    EXPECT_EQ(std::string(64,'x')+"a",name);
    };
  dialog.exec();
  }

TEST(GamepadDialog, OutsideClicksAndDragsNeverConfirmSave) {
  std::string name = "Original";
  QueuedDialog<GameMenu::SavNameDialog> dialog(name);
  dialog.queue = [&] {
    dialog.cells[GamepadTextInput::Save] = Tempest::Rect(0,0,10,10);
    Tempest::MouseEvent inside(1,1,Tempest::Event::ButtonLeft);
    Tempest::MouseEvent outside(20,20,Tempest::Event::ButtonLeft);
    dialog.mouseDownEvent(outside);
    dialog.mouseUpEvent(inside);
    EXPECT_TRUE(dialog.isOpen());
    dialog.mouseDownEvent(inside);
    dialog.mouseUpEvent(outside);
    EXPECT_TRUE(dialog.isOpen());
    EXPECT_FALSE(dialog.decision.isAccepted());
    dialog.onGamepad(KeyCodec::Escape);
    EXPECT_EQ("Original",name);
    };
  dialog.exec();
  }

TEST(GamepadDialog, MouseKeyDisabledByMixedSourceLayoutChangeIsIgnored) {
  std::string name;
  QueuedDialog<GameMenu::SavNameDialog> dialog(name,true);
  dialog.queue = [&] {
    dialog.cells[32] = Tempest::Rect(0,0,10,10);
    Tempest::MouseEvent click(1,1,Tempest::Event::ButtonLeft);
    dialog.mouseDownEvent(click);
    selectCell(dialog,GamepadTextInput::Language);
    dialog.onGamepad(KeyCodec::ActionGeneric);
    EXPECT_FALSE(dialog.input.enabled(32));
    dialog.mouseUpEvent(click);
    EXPECT_TRUE(name.empty());
    EXPECT_TRUE(dialog.isOpen());
    dialog.onGamepad(KeyCodec::Escape);
    };
  dialog.exec();
  }

TEST(GamepadDialog, HeldCrossCannotRepeatACharacterOrSaveAfterMovingSelection) {
  DialogQueue input;
  std::string name;
  QueuedDialog<GameMenu::SavNameDialog> dialog(name);
  MenuModalInput::Scope scope(input.modal,[&](KeyCodec::Action action) { dialog.onGamepad(action); });
  input.sync();
  dialog.queue = [&] {
    selectCell(dialog,0);
    input.edge(Button::South,true);
    EXPECT_EQ("a",name);
    EXPECT_EQ(Command::Type::None,input.edge(Button::South,true).type);
    EXPECT_EQ(Command::Type::None,input.router.update(input.state,1000).type);
    EXPECT_EQ("a",name);
    selectCell(dialog,GamepadTextInput::Save);
    EXPECT_EQ(Command::Type::None,input.edge(Button::South,true).type);
    EXPECT_EQ(Command::Type::None,input.router.update(input.state,2000).type);
    EXPECT_TRUE(dialog.isOpen());
    EXPECT_FALSE(dialog.decision.isAccepted());
    input.edge(Button::South,false);
    input.edge(Button::South,true);
    EXPECT_TRUE(dialog.decision.isAccepted());
    EXPECT_EQ("a",name);
    };
  dialog.exec();
  }

TEST(GamepadDialog, ClosedJournalContentIgnoresQueuedScrolling) {
  GameMenu::Item text;
  text.scroll = 5;
  QueuedDialog<GameMenu::ListContentDialog> dialog(text);
  DialogQueue input;
  {
    MenuModalInput::Scope scope(input.modal,[&](KeyCodec::Action action) { dialog.onGamepad(action); });
    input.sync();
    dialog.queue = [&] {
      input.edge(Button::DpadDown,true);
      EXPECT_EQ(6,text.scroll);
      input.edge(Button::DpadDown,false);
      input.edge(Button::Menu,true);
      EXPECT_FALSE(dialog.isOpen());
      input.edge(Button::DpadDown,true);
      EXPECT_EQ(6,text.scroll);
      input.edge(Button::DpadDown,false);
      input.edge(Button::DpadUp,true);
      EXPECT_EQ(6,text.scroll);
      EXPECT_EQ(1u,input.revision);
      };
    dialog.exec();
    EXPECT_EQ(1u,input.revision);
    }
  EXPECT_EQ(2u,input.revision);
  }

TEST(GamepadDialog, ClosedKeyEditorIgnoresQueuedConfirmWithoutBinding) {
  QueuedDialog<GameMenu::KeyEditDialog> dialog;
  DialogQueue input;
  {
    MenuModalInput::Scope scope(input.modal,[&](KeyCodec::Action action) { dialog.onGamepad(action); });
    input.sync();
    dialog.queue = [&] {
      input.edge(Button::East,true);
      EXPECT_FALSE(dialog.isOpen());
      EXPECT_EQ(Command::Type::Navigate,input.edge(Button::South,true).type);
      EXPECT_FALSE(dialog.isOpen());
      EXPECT_EQ(Tempest::Event::K_ESCAPE,dialog.key);
      EXPECT_EQ(Tempest::Event::ButtonNone,dialog.mkey);
      EXPECT_EQ(1u,input.revision);
      };
    dialog.exec();
    EXPECT_EQ(1u,input.revision);
    }
  EXPECT_EQ(2u,input.revision);
  }
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc,argv);
  Tempest::Application application;
  return RUN_ALL_TESTS();
  }
