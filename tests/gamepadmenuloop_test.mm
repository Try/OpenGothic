// Compile with test-only -fno-access-control to use the actual private dialogs.
// This fixture covers the native event-loop boundary and production input
// routing/queue. It does not construct MainWindow, a GPU device or game assets.
#include "ui/gamemenu.cpp"
#include "ui/gamepadmenuinput.h"

#include <Tempest/Application>
#include <Tempest/SystemApi>

#import <dispatch/dispatch.h>
#include <pthread.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <functional>

// Keep CoreFoundation/MacTypes out of the translation unit that includes the
// real game menu implementation: both platforms define Point/Rect/Fixed.
extern "C" void* gamepadMenuLoopStartWatchdog(void (*callback)(void*), void* context);
extern "C" void gamepadMenuLoopStopWatchdog(void* timer);

namespace {
using Pad = Tempest::Gamepad;
using Button = Pad::Button;
using Command = GamepadInput::Command;

template<class Base>
class NativeDialog final : public Base {
  public:
    using Base::Base;
    std::function<void()> shown;
  protected:
    void showEvent() override {
      if(shown)
        shown();
      }
    void paintEvent(Tempest::PaintEvent&) override {}
    void paintShadow(Tempest::PaintEvent&) override {}
  };

struct DepthScope final {
  explicit DepthScope(unsigned& depth):depth(depth) { ++depth; }
  ~DepthScope() { --depth; }
  unsigned& depth;
  };

struct BoundaryObserver final {
  void onBoundary() { ++calls; }
  unsigned calls = 0;
  };

class NativeMenuLoop final {
  public:
    explicit NativeMenuLoop(bool direct):direct(direct),modal(revision) {
      router.selectDevice(Device,{});
      route(router.setContext(screen(),state));
      bindObserver();
      Tempest::Application::onEventLoop().bind(this,&NativeMenuLoop::onBoundary);
      }

    ~NativeMenuLoop() {
      Tempest::Application::onEventLoop().ubind(this,&NativeMenuLoop::onBoundary);
      unbindObserver();
      gamepadMenuLoopStopWatchdog(watchdog);
      }

    void start() {
      // Initialize the real AppKit backend before the bounded watchdog starts.
      expect(Tempest::Application::isRunning(),"application initially running");
      watchdog = gamepadMenuLoopStartWatchdog([](void* context) {
        static_cast<NativeMenuLoop*>(context)->onWatchdog();
        },this);
      expect(watchdog!=nullptr,"native watchdog was created");
      postNative("initial-frame",[this] {
        edge(Button::South,true);
        expect(!menu.empty(),"initial frame queued its menu action");
        expect(parentOpens==0,"frame did not open a dialog while enqueueing");
        if(direct)
          pump(); // Negative mutation: the old render/GCD entry point.
        initialFrameReturned = true;
        });
      }

    int finish(int applicationResult) {
      expect(applicationResult==0,"Application::exec returned zero");
      expect(quitRequested,"explicit quit callback ran");
      const auto countAtReturn = boundaryCalls;
      // Both processEvents and the outer timer path must suppress the signal
      // once the native exit flag has been set.
      Tempest::Application::processEvents();
      Tempest::Application::processEvents();
      expect(boundaryCalls==countAtReturn,"no boundary after exec returned");
      expect(boundaryCalls==boundariesAtQuit,"quit native poll emitted no boundary");
      expect(!watchdogFired,"native watchdog did not fire");
      expect(parentOpens==1 && childOpens==1,"both modal levels opened once");
      expect(parentMoves==1 && childScrolls==1,"input returned to the correct modal level");
      expect(parentFrames>0 && parentHid>0,"parent received GCD frame and HID callbacks");
      expect(childFrames>0 && childHid>0,"child received GCD frame and HID callbacks");
      expect(saveQueuedConfirm,"cancel left a distinct queued confirm before modal unwind");
      expect(saveClosedConfirm==0,"next boundary discarded confirm from the old save screen");
      expect(maxBoundaryDepth>=3,"event-loop signal reentered for parent and child");
      expect(revision==6,"each modal entry and return changed revision");
      expect(!modal.active() && menu.empty(),"modal stack and menu queue unwound");
      std::printf("RESULT mode=%s failures=%u boundaries=%u max-depth=%u "
                  "parent-frame=%u parent-hid=%u child-frame=%u child-hid=%u "
                  "save-queued-confirm=%u save-closed-confirm=%u revision=%llu watchdog=%u\n",
                  direct ? "main-queue-negative" : "event-loop-boundary",failures,
                  boundaryCalls,maxBoundaryDepth,parentFrames,parentHid,childFrames,
                  childHid,unsigned(saveQueuedConfirm),saveClosedConfirm,
                  static_cast<unsigned long long>(revision),
                  unsigned(watchdogFired));
      std::fflush(stdout);
      return failures==0 ? 0 : 1;
      }

  private:
    static constexpr Pad::DeviceId Device = 7;
    enum class Stage { Parent, Save, Rebind, Quit };

    void expect(bool value, const char* message) {
      if(value)
        return;
      ++failures;
      std::fprintf(stderr,"FAIL: %s\n",message);
      std::fflush(stderr);
      }

    void trace(const char* message) {
      std::printf("TRACE %s main-queue=%u boundary-depth=%u revision=%llu\n",message,
                  mainQueueDepth,boundaryDepth,static_cast<unsigned long long>(revision));
      std::fflush(stdout);
      }

    void postNative(const char* name, std::function<void()> callback) {
      // Synthetic HID/frame work uses the same serial main queue as the native
      // backend. Pending blocks only capture this fixture, never a stack dialog.
      dispatch_async(dispatch_get_main_queue(),^{
        DepthScope scope(mainQueueDepth);
        if(quitRequested || watchdogFired)
          return;
        expect(pthread_main_np()!=0,"native callback is on the UI thread");
        trace(name);
        callback();
        });
      }

    GamepadInput::Screen screen() const {
      return {GamepadInput::Context::Menu,revision};
      }

    void route(const Command& command) {
      if(command.type==Command::Type::Cancel || command.type==Command::Type::CancelAll)
        menu.clear();
      else
        menu.push(command,router.screen(),router.device());
      }

    void processGamepad() {
      route(router.setContext(screen(),state));
      route(router.update(state,Tempest::Application::tickCount()));
      }

    void edge(Button button, bool down) {
      const auto mask = uint32_t(1)<<uint8_t(button);
      if(down)
        state.buttons |= mask;
      else
        state.buttons &= ~mask;
      route(router.setContext(screen(),state));
      const Pad::Event event{down ? Pad::Event::Type::ButtonDown : Pad::Event::Type::ButtonUp,
                             Device,button};
      route(router.process(event,Tempest::Application::tickCount()));
      }

    void releaseAll() {
      const auto held = state.buttons;
      state = {};
      route(router.setContext(screen(),state));
      for(uint8_t i=0;i<uint8_t(Button::Count);++i) {
        if((held & (uint32_t(1)<<i))==0)
          continue;
        const Pad::Event event{Pad::Event::Type::ButtonUp,Device,Button(i)};
        route(router.process(event,Tempest::Application::tickCount()));
        }
      processGamepad();
      }

    void onBoundary() {
      ++boundaryCalls;
      DepthScope scope(boundaryDepth);
      maxBoundaryDepth = std::max(maxBoundaryDepth,boundaryDepth);
      expect(!quitRequested,"event-loop signal was not emitted after quit");
      if(observerUnbound)
        expect(observer.calls==observerAtUnbind,"unbound observer stayed inactive");
      if(!direct)
        pump();
      if(stage==Stage::Rebind && rebindFrameReturned && observer.calls>observerAtRebind) {
        trace("observer-rebound");
        expect(menu.empty(),"preflight discarded the old save screen's confirm");
        stage = Stage::Quit;
        postNative("quit-native-poll",[this] {
          expect(!modal.active(),"quit starts after all dialogs return");
          unbindObserver();
          menu.clear();
          boundariesAtQuit = boundaryCalls;
          quitRequested = true;
          Tempest::SystemApi::exit();
          });
        }
      }

    void pump() {
      if(menu.empty() && !menu.overflowed())
        return;
      // This is the MainWindow boundary adapter. The actual FIFO, stale
      // Screen/Device validation and router are production implementations.
      // One command per emission lets native release events make progress even
      // when handling an action takes longer than the navigation repeat delay.
      processGamepad();
      if(menu.overflowed()) {
        menu.clear();
        router.reset(state);
        return;
        }
      const auto command = menu.pop(screen(),router.device());
      if(command.type==Command::Type::None)
        return;
      if(modal.active())
        modal.dispatch(command.action);
      else if(command.action==KeyCodec::ActionGeneric && stage==Stage::Parent)
        openParent();
      else if(command.action==KeyCodec::ActionGeneric && stage==Stage::Save)
        openSave();
      else
        expect(false,"unexpected root menu action");
      }

    void openParent() {
      expect(mainQueueDepth==0,"parent Dialog::exec starts outside the GCD frame");
      expect(initialFrameReturned,"initial GCD frame returned before Dialog::exec");
      ++parentOpens;
      trace("parent-open");
      NativeDialog<Tempest::Dialog> dialog;
      parent = &dialog;
      dialog.shown = [this] {
        postNative("parent-frame",[this] {
          expect(parent!=nullptr && parent->isOpen(),"frame runs inside parent Dialog::exec");
          ++parentFrames;
          });
        postNative("parent-hid-open-child",[this] {
          ++parentHid;
          releaseAll();
          edge(Button::South,true);
          });
        };
      {
      MenuModalInput::Scope scope(modal,[this,&dialog](KeyCodec::Action action) {
        if(!dialog.isOpen())
          return;
        if(action==KeyCodec::ActionGeneric)
          openChild();
        else if(action==KeyCodec::Back) {
          ++parentMoves;
          postNative("parent-hid-close",[this] {
            ++parentHid;
            edge(Button::East,true);
            });
          }
        else if(action==KeyCodec::Escape)
          dialog.close();
        });
      dialog.exec();
      }
      parent = nullptr;
      trace("parent-return");
      if(watchdogFired)
        return;
      expect(child==nullptr && !modal.active(),"child and parent scopes returned");
      expect(revision==4,"two modal levels completed four revision transitions");
      unbindObserver();
      expect(observerAtUnbind>0,"bound observer received actual native boundaries");
      stage = Stage::Save;
      postNative("save-open-frame",[this] {
        releaseAll();
        edge(Button::South,true);
        });
      }

    void openChild() {
      expect(mainQueueDepth==0,"child Dialog::exec starts outside a GCD callback");
      expect(parent!=nullptr && parent->isOpen(),"parent stays open under child");
      ++childOpens;
      trace("child-open");
      GameMenu::Item text;
      text.scroll = 0;
      NativeDialog<GameMenu::ListContentDialog> dialog(text);
      child = &dialog;
      dialog.shown = [this] {
        postNative("child-frame",[this] {
          expect(child!=nullptr && child->isOpen(),"frame runs inside child Dialog::exec");
          ++childFrames;
          });
        postNative("child-hid-scroll",[this] {
          ++childHid;
          releaseAll();
          edge(Button::DpadDown,true);
          edge(Button::DpadDown,false);
          });
        };
      {
      MenuModalInput::Scope scope(modal,[this,&dialog,&text](KeyCodec::Action action) {
        dialog.onGamepad(action);
        if(action==KeyCodec::Back) {
          ++childScrolls;
          expect(text.scroll==1,"actual ListContentDialog handled scroll");
          postNative("child-hid-close",[this] {
            ++childHid;
            edge(Button::East,true);
            });
          }
        });
      dialog.exec();
      }
      child = nullptr;
      trace("child-return");
      if(watchdogFired)
        return;
      expect(parent!=nullptr && parent->isOpen(),"child close leaves parent open");
      expect(modal.active() && revision==3,"parent handler restored after child return");
      postNative("parent-hid-resume",[this] {
        ++parentHid;
        releaseAll();
        edge(Button::DpadDown,true);
        edge(Button::DpadDown,false);
        });
      }

    void openSave() {
      expect(mainQueueDepth==0,"save Dialog::exec starts outside a GCD callback");
      trace("save-open");
      std::string name = "Existing save";
      NativeDialog<GameMenu::SavNameDialog> dialog(name);
      name = "Pending keyboard edit";
      save = &dialog;
      dialog.shown = [this] {
        postNative("save-hid-cancel-confirm",[this] {
          releaseAll();
          edge(Button::East,true);
          edge(Button::South,true);
          });
        };
      {
      MenuModalInput::Scope scope(modal,[this,&dialog,&name](KeyCodec::Action action) {
        const bool wasOpen = dialog.isOpen();
        dialog.onGamepad(action);
        if(!wasOpen && action==KeyCodec::ActionGeneric)
          ++saveClosedConfirm;
        if(action==KeyCodec::Escape || !wasOpen) {
          // Native-loop delivery may leave mixed input behind the terminal edge.
          Tempest::KeyEvent back(Tempest::Event::K_Back);
          Tempest::KeyEvent character(uint32_t('x'));
          Tempest::KeyEvent enter(Tempest::Event::K_Return);
          Tempest::MouseEvent click(1,1,Tempest::Event::ButtonLeft);
          dialog.keyUpEvent(back);
          dialog.keyUpEvent(character);
          dialog.keyUpEvent(enter);
          dialog.cells[GamepadTextInput::Save] = Tempest::Rect(0,0,10,10);
          dialog.pressedCell = GamepadTextInput::Save;
          dialog.mouseUpEvent(click);
          expect(!dialog.isOpen(),"save cancel closes the actual overlay");
          expect(!dialog.decision.isAccepted(),"cancelled save cannot become accepted");
          expect(name=="Existing save","cancel restores the original save name");
          }
        if(action==KeyCodec::Escape)
          saveQueuedConfirm = !menu.empty();
        });
      dialog.exec();
      }
      save = nullptr;
      trace("save-return");
      if(watchdogFired)
        return;
      expect(saveQueuedConfirm && !dialog.decision.isAccepted(),"queued confirm kept cancellation terminal");
      expect(observer.calls==observerAtUnbind,"observer stayed unbound across save nested loop");
      bindObserver();
      observerAtRebind = observer.calls;
      stage = Stage::Rebind;
      postNative("rebind-probe-frame",[this] { rebindFrameReturned = true; });
      }

    void bindObserver() {
      Tempest::Application::onEventLoop().bind(&observer,&BoundaryObserver::onBoundary);
      observerUnbound = false;
      }

    void unbindObserver() {
      if(observerUnbound)
        return;
      Tempest::Application::onEventLoop().ubind(&observer,&BoundaryObserver::onBoundary);
      observerAtUnbind = observer.calls;
      observerUnbound = true;
      }

    void onWatchdog() {
      watchdogFired = true;
      std::fprintf(stderr,"WATCHDOG: native main queue stalled, main-queue=%u "
                          "parent-frame=%u parent-hid=%u child-frame=%u child-hid=%u\n",
                          mainQueueDepth,parentFrames,parentHid,childFrames,childHid);
      std::fflush(stderr);
      menu.clear();
      if(child!=nullptr)
        child->close();
      if(save!=nullptr)
        save->close();
      if(parent!=nullptr)
        parent->close();
      boundariesAtQuit = boundaryCalls;
      quitRequested = true;
      Tempest::SystemApi::exit();
      }

    const bool direct;
    uint64_t revision = 0;
    MenuModalInput modal;
    GamepadInput router;
    GamepadMenuInput menu;
    Pad::State state;
    BoundaryObserver observer;
    void* watchdog = nullptr;
    Tempest::Dialog* parent = nullptr;
    Tempest::Dialog* child = nullptr;
    Tempest::Dialog* save = nullptr;
    Stage stage = Stage::Parent;
    unsigned failures = 0;
    unsigned mainQueueDepth = 0;
    unsigned boundaryDepth = 0;
    unsigned maxBoundaryDepth = 0;
    unsigned boundaryCalls = 0;
    unsigned boundariesAtQuit = 0;
    unsigned parentOpens = 0;
    unsigned childOpens = 0;
    unsigned parentMoves = 0;
    unsigned childScrolls = 0;
    unsigned parentFrames = 0;
    unsigned parentHid = 0;
    unsigned childFrames = 0;
    unsigned childHid = 0;
    unsigned saveClosedConfirm = 0;
    unsigned observerAtUnbind = 0;
    unsigned observerAtRebind = 0;
    bool observerUnbound = true;
    bool saveQueuedConfirm = false;
    bool initialFrameReturned = false;
    bool rebindFrameReturned = false;
    bool quitRequested = false;
    bool watchdogFired = false;
  };
}

int main(int argc, char** argv) {
  const bool direct = argc==2 && std::strcmp(argv[1],"--dispatch-from-main-queue")==0;
  if(argc>1 && !direct) {
    std::fprintf(stderr,"Usage: %s [--dispatch-from-main-queue]\n",argv[0]);
    return 2;
    }
  @autoreleasepool {
    Tempest::Application application;
    NativeMenuLoop fixture(direct);
    fixture.start();
    return fixture.finish(application.exec());
    }
  }
