// Test-only -fno-access-control calls the exact private delegating constructor
// without constructing a MenuRoot, VM, renderer or game assets.
#include "ui/menuroot.h"

#include <Tempest/Dialog>
#include <Tempest/Window>
#include <gtest/gtest.h>

#include <functional>

namespace {
class QueuedModal final : public Tempest::Dialog {
  public:
    std::function<void()> beforeFirstInput;
  protected:
    void showEvent() override {
      EXPECT_TRUE(isOpen());
      if(beforeFirstInput)
        beforeFirstInput();
      close();
      }
    void paintEvent(Tempest::PaintEvent&) override {}
    void paintShadow(Tempest::PaintEvent&) override {}
  };

struct CachedMenu final {
  CachedMenu() {
    window.addWidget(&menu);
    clear();
    }
  ~CachedMenu() {
    window.takeWidget(&menu);
    }
  void clear() {
    menu.astate.needToUpdate = false;
    window.astate.needToUpdate = false;
    }
  bool dirty() const { return window.astate.needToUpdate; }

  Tempest::Window window;
  Tempest::Widget menu;
  uint64_t revision = 0;
  MenuModalInput input{revision};
  };

TEST(GamepadModalRedraw, RawModalScopeLeavesRealCachedWindowCleanBeforeFirstInput) {
  CachedMenu cached;
  EXPECT_FALSE(cached.dirty());
  QueuedModal dialog;
  dialog.beforeFirstInput = [&] {
    EXPECT_TRUE(cached.input.active());
    EXPECT_EQ(cached.revision,1u);
    EXPECT_FALSE(cached.dirty());
    };
  {
  MenuModalInput::Scope scope(cached.input,{});
  EXPECT_FALSE(cached.dirty());
  dialog.exec();
  }
  EXPECT_FALSE(cached.dirty());
  EXPECT_EQ(cached.revision,2u);
  }

TEST(GamepadModalRedraw, ProductionScopeInvalidatesRealWindowBeforeExecAndEveryNestedEntry) {
  CachedMenu cached;
  EXPECT_FALSE(cached.dirty());
  {
  MenuRoot::ModalInputScope outer(cached.menu,cached.input,{});
  EXPECT_TRUE(cached.dirty());
  EXPECT_EQ(cached.revision,1u);
  QueuedModal dialog;
  dialog.beforeFirstInput = [&] {
    EXPECT_TRUE(cached.dirty());
    EXPECT_TRUE(cached.input.active());
    EXPECT_EQ(cached.revision,1u);
    // Simulate completion of the cached frame while the parent modal is open.
    cached.clear();
    EXPECT_FALSE(cached.dirty());
    {
    MenuRoot::ModalInputScope nested(cached.menu,cached.input,{});
    EXPECT_TRUE(cached.dirty());
    EXPECT_EQ(cached.revision,2u);
    QueuedModal child;
    child.beforeFirstInput = [&] {
      EXPECT_TRUE(cached.dirty());
      EXPECT_EQ(cached.revision,2u);
      };
    child.exec();
    }
    EXPECT_EQ(cached.revision,3u);
    EXPECT_TRUE(cached.input.active());
    };
  dialog.exec();
  }
  EXPECT_EQ(cached.revision,4u);
  EXPECT_FALSE(cached.input.active());
  }
}
