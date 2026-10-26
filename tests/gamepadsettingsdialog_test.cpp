// Test-only -fno-access-control permits selection without loading game fonts.
// All decisions and late input still pass through the real dialog handlers.
#include "ui/gamepadsettingsmenu.h"
#include "ui/menuinput.h"

#include <Tempest/Application>
#include <gtest/gtest.h>

#include <functional>

namespace {
struct DialogBoundary final {
  explicit DialogBoundary(GamepadSettingsMenu& dialog):dialog(dialog) {
    }

  ~DialogBoundary() {
    Tempest::Application::onEventLoop().ubind(this,&DialogBoundary::run);
    }

  void run() {
    // Dialog::exec pumps processEvents, which signals this boundary. Timers
    // belong to the outer native timer callback and do not run in that loop.
    Tempest::Application::onEventLoop().ubind(this,&DialogBoundary::run);
    ran = true;
    EXPECT_TRUE(dialog.isOpen());
    if(queue)
      queue();
    dialog.close();
    }

  void exec() {
    Tempest::Application::onEventLoop().bind(this,&DialogBoundary::run);
    dialog.exec();
    Tempest::Application::onEventLoop().ubind(this,&DialogBoundary::run);
    EXPECT_TRUE(ran);
    EXPECT_FALSE(dialog.isOpen());
    }

  GamepadSettingsMenu& dialog;
  std::function<void()> queue;
  bool ran = false;
  };

void lateEdits(GamepadSettingsMenu& dialog) {
  dialog.onGamepad(KeyCodec::Right);
  dialog.onGamepad(KeyCodec::ActionGeneric);
  Tempest::KeyEvent enter(Tempest::Event::K_Return);
  dialog.keyDownEvent(enter);
  dialog.keyUpEvent(enter);
  Tempest::MouseEvent click(0,0,Tempest::Event::ButtonLeft);
  dialog.mouseDownEvent(click);
  Tempest::MouseEvent wheel(0,0,Tempest::Event::ButtonNone,Tempest::Event::M_NoModifier,1);
  dialog.mouseWheelEvent(wheel);
  }

TEST(GamepadSettingsDialog, CanvasTracksParentShrinkWithoutChangingDialogSize) {
  Tempest::Widget viewport;
  viewport.resize(1280,720);
  viewport.setMargins(Tempest::Margin(0));
  GamepadSettingsMenu dialog(GamepadSettings(),false);
  dialog.resize(1280,720);
  dialog.setMargins(Tempest::Margin(0));
  viewport.addWidget(&dialog);
  EXPECT_EQ(dialog.canvasRect(),Tempest::Rect(0,0,1280,720));
  viewport.resize(640,360);
  EXPECT_EQ(dialog.size(),Tempest::Size(1280,720));
  EXPECT_EQ(dialog.canvasRect(),Tempest::Rect(0,0,640,360));
  dialog.setPosition(20,30);
  EXPECT_EQ(dialog.canvasRect(),Tempest::Rect(0,0,620,330));
  viewport.resize(1920,1080);
  EXPECT_EQ(dialog.canvasRect(),Tempest::Rect(0,0,1280,720));
  viewport.takeWidget(&dialog);
  }

TEST(GamepadSettingsDialog, SafeInsetsAreNotDoubledAndSurviveOwnerShrink) {
  Tempest::Widget viewport;
  viewport.resize(844,390);
  viewport.setMargins(Tempest::Margin(47,10,12,21));
  GamepadSettingsMenu dialog(GamepadSettings(),false);
  dialog.resize(viewport.size());
  dialog.setMargins(viewport.margins());
  viewport.addWidget(&dialog);
  EXPECT_EQ(dialog.canvasRect(),Tempest::Rect(47,12,787,357));
  viewport.resize(640,300);
  EXPECT_EQ(dialog.canvasRect(),Tempest::Rect(47,12,583,267));
  viewport.takeWidget(&dialog);
  }

TEST(GamepadSettingsDialog, MetricsUpdateCurrentInsetsAndTinyCanvasRemainsBounded) {
  GamepadSettingsMenu dialog(GamepadSettings(),false);
  dialog.resize(844,390);
  Tempest::DisplayMetricsEvent changed(Tempest::Margin(10,47,0,15),1.f);
  dialog.metricsEvent(changed);
  EXPECT_EQ(dialog.canvasRect(),Tempest::Rect(10,0,787,375));
  dialog.resize(5,5);
  Tempest::DisplayMetricsEvent tiny(Tempest::Margin(100),1.f);
  dialog.metricsEvent(tiny);
  EXPECT_EQ(dialog.canvasRect(),Tempest::Rect(4,4,1,1));
  }

TEST(GamepadSettingsDialog, CancelThenQueuedConfirmCannotAcceptBeforeScopeUnwinds) {
  GamepadSettings original;
  original.sensitivityX = 150;
  GamepadSettingsMenu dialog(original,false);
  DialogBoundary loop(dialog);
  uint64_t revision = 0;
  MenuModalInput modal(revision);
  {
  MenuModalInput::Scope scope(modal,[&](KeyCodec::Action action) { dialog.onGamepad(action); });
  loop.queue = [&] {
    dialog.input.select(2);
    modal.dispatch(KeyCodec::Right);
    EXPECT_EQ(dialog.settings().sensitivityX,155);
    dialog.input.select(GamepadSettingsInput::Apply);
    modal.dispatch(KeyCodec::Escape);
    EXPECT_FALSE(dialog.isOpen());
    const auto cancelled = dialog.settings();
    modal.dispatch(KeyCodec::ActionGeneric);
    lateEdits(dialog);
    EXPECT_FALSE(dialog.accepted());
    EXPECT_EQ(dialog.settings(),cancelled);
    EXPECT_EQ(revision,1u);
    };
  loop.exec();
  EXPECT_TRUE(modal.active());
  EXPECT_FALSE(dialog.accepted());
  }
  EXPECT_EQ(revision,2u);
  EXPECT_EQ(original.sensitivityX,150);
  }

TEST(GamepadSettingsDialog, ApplyThenQueuedCancelAndEditsPreserveAcceptedDraft) {
  GamepadSettingsMenu dialog(GamepadSettings(),true);
  DialogBoundary loop(dialog);
  loop.queue = [&] {
    dialog.input.select(4);
    dialog.onGamepad(KeyCodec::Right);
    EXPECT_TRUE(dialog.settings().invertY);
    dialog.input.select(GamepadSettingsInput::Apply);
    Tempest::KeyEvent enter(Tempest::Event::K_Return);
    dialog.keyDownEvent(enter);
    EXPECT_TRUE(dialog.accepted());
    EXPECT_FALSE(dialog.isOpen());
    const auto accepted = dialog.settings();
    dialog.onGamepad(KeyCodec::Escape);
    Tempest::KeyEvent escape(Tempest::Event::K_ESCAPE);
    dialog.keyDownEvent(escape);
    lateEdits(dialog);
    EXPECT_TRUE(dialog.accepted());
    EXPECT_EQ(dialog.settings(),accepted);
    };
  loop.exec();
  EXPECT_TRUE(dialog.accepted());
  EXPECT_TRUE(dialog.settings().invertY);
  }

TEST(GamepadSettingsDialog, ResetOnlyChangesDraftAndCanBeCancelled) {
  GamepadSettings original;
  original.invertY = true;
  original.bindings[size_t(GamepadSettings::Pad::Button::South)] = KeyCodec::Map;
  GamepadSettingsMenu dialog(original,false);
  DialogBoundary loop(dialog);
  loop.queue = [&] {
    dialog.input.select(GamepadSettingsInput::Reset);
    Tempest::KeyEvent enter(Tempest::Event::K_Return);
    dialog.keyDownEvent(enter);
    EXPECT_TRUE(dialog.isOpen());
    EXPECT_FALSE(dialog.accepted());
    EXPECT_EQ(dialog.settings(),GamepadSettings());
    EXPECT_TRUE(original.invertY);
    EXPECT_EQ(original.bindings[size_t(GamepadSettings::Pad::Button::South)],KeyCodec::Map);
    dialog.onGamepad(KeyCodec::Escape);
    lateEdits(dialog);
    EXPECT_FALSE(dialog.accepted());
    };
  loop.exec();
  EXPECT_FALSE(dialog.accepted());
  EXPECT_TRUE(original.invertY);
  }
}
