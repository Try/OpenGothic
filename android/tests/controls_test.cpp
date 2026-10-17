// Host-side test of the touch controls state machine. It has no Android dependencies:
//   c++ -std=c++17 -I common android/tests/controls_test.cpp common/ui/mobilecontrolsmodel.cpp && ./a.out
#include "ui/mobilecontrolsmodel.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>

using namespace Mobile;

#define CHECK(cond) \
  do { \
    if(!(cond)) { \
      std::fprintf(stderr,"%s:%d: check failed: %s\n",__FILE__,__LINE__,#cond); \
      std::abort(); \
      } \
    } while(false)

namespace {

constexpr uint64_t T0 = 1000;

struct Size {
  float w, h;
  };

const Size screens[] = {{1920,1080}, {2400,1080}, {1280,720}, {2560,1600}};

const Button& button(const Controls& c, Key key) {
  for(const auto& b:c.buttons()) {
    if(b.key==key && b.special==Special::None && b.character==0)
      return b;
    }
  CHECK(false);
  return c.buttons()[0];
  }

const Button& special(const Controls& c, int id) {
  for(const auto& b:c.buttons()) {
    if(b.special==id)
      return b;
    }
  CHECK(false);
  return c.buttons()[0];
  }

float cx(const Rect& r) { return r.x+r.w/2; }
float cy(const Rect& r) { return r.y+r.h/2; }

void press(Controls& c, Key key, int id=1) {
  auto r = button(c,key).rect;
  c.down(id,cx(r),cy(r),T0);
  }

void lift(Controls& c, Key key, int id=1) {
  auto r = button(c,key).rect;
  c.up(id,cx(r),cy(r));
  }

void tap(Controls& c, const Rect& r, int id=1) {
  c.down(id,cx(r),cy(r),T0);
  c.up(id,cx(r),cy(r));
  }

// The joystick starts where the finger lands, so a push is a move from that point
void stickPush(Controls& c, int id, float dx, float dy) {
  const auto  stick = c.stickArea();
  const float x     = stick.x+stick.w/2;
  const float y     = stick.y+stick.h/2;
  c.down(id,x,y,T0);
  c.move(id,x+dx*stick.w,y+dy*stick.h);
  }

bool has(const std::vector<Event>& v, Key key, Event::Kind kind) {
  for(const auto& e:v) {
    if(e.key==key && e.kind==kind)
      return true;
    }
  return false;
  }

bool has(const std::vector<Event>& v, Event::Kind kind) {
  for(const auto& e:v) {
    if(e.kind==kind)
      return true;
    }
  return false;
  }

void noConfirm(const std::vector<Event>& events) {
  for(const auto& e:events) {
    CHECK(e.key!=Key::Enter || e.kind==Event::Look || e.kind==Event::Tap || e.kind==Event::Scroll ||
          e.kind==Event::Special || e.kind==Event::MenuTouch);
    }
  }

bool overlaps(const Rect& a, const Rect& b) {
  return a.x<b.x+b.w && a.x+a.w>b.x && a.y<b.y+b.h && a.y+a.h>b.y;
  }

void checkLayout(const Controls& c, Size screen) {
  const auto& buttons = c.buttons();
  for(size_t i=0; i<buttons.size(); ++i) {
    const auto& a = buttons[i].rect;
    // The text button of the inventory is the only wide one
    if(buttons[i].special==Special::None && buttons[i].character==0 && buttons[i].key<Key::One && buttons[i].label!="USE")
      CHECK(std::abs(a.w-a.h)<0.01f);
    CHECK(a.x>=0 && a.y>=0 && a.x+a.w<=screen.w && a.y+a.h<=screen.h);
    for(size_t j=i+1; j<buttons.size(); ++j)
      CHECK(!overlaps(a,buttons[j].rect));
    }
  }

void menuSwipeDoesNotConfirm() {
  Controls c;
  c.dialogPad(true);
  c.configure(2400,1080,Mode::Dialog);

  // A menu swipe changes selection only, even across many move events.
  for(int i=0; i<30; ++i) {
    c.down(1,1200,400,T0);
    c.move(1,1200,300);
    c.up(1,1200,200);
    auto e = c.drain();
    noConfirm(e);
    CHECK(has(e,Key::Up,Event::Down));
    }
  // A tap on the blank space is only reported, the dialog decides what it means.
  c.down(1,1200,400,T0);
  c.up(1,1200,400);
  auto tapped = c.drain();
  int  taps   = 0;
  for(const auto& e:tapped)
    taps += e.kind==Event::Tap ? 1 : 0;
  CHECK(taps==1);
  noConfirm(tapped);

  // A finger starting on blank space must never activate a button by sliding over it.
  auto ok = button(c,Key::Enter).rect;
  c.down(3,1200,400,T0);
  c.move(3,ok.x+5,ok.y+5);
  c.up(3,ok.x+5,ok.y+5);
  noConfirm(c.drain());
  }

void explicitConfirmation() {
  Controls c;
  c.dialogPad(true);
  c.configure(2400,1080,Mode::Dialog);

  // Only an intentional release inside OK confirms; down, drag and cancel do not.
  press(c,Key::Enter);
  CHECK(c.drain().empty());
  lift(c,Key::Enter);
  auto e = c.drain();
  CHECK(e.size()==2 && e[0].kind==Event::Down && e[0].key==Key::Enter && e[1].kind==Event::Up);

  press(c,Key::Enter);
  c.move(1,0,0);
  lift(c,Key::Enter);
  CHECK(c.drain().empty());

  press(c,Key::Enter);
  c.resetPointers();
  lift(c,Key::Enter);
  CHECK(c.drain().empty());

  // Pointer IDs are reusable after a cancel.
  press(c,Key::Enter);
  lift(c,Key::Enter);
  CHECK(c.drain().size()==2);

  // Navigation repeats at a bounded rate, never repeats confirm.
  press(c,Key::Down);
  noConfirm(c.drain());
  c.tick(T0+399);
  CHECK(c.drain().empty());
  c.tick(T0+400);
  e = c.drain();
  CHECK(e.size()==2 && e[0].key==Key::Down);
  lift(c,Key::Down);
  CHECK(c.drain().empty());

  press(c,Key::Enter);
  c.tick(T0+5000);
  CHECK(c.drain().empty());
  lift(c,Key::Enter);
  CHECK(c.drain().size()==2);
  }

void gameMultitouch() {
  Controls c;
  c.configure(2400,1080,Mode::Game);
  c.drain();

  // Two simultaneous touches retain the key until both release.
  press(c,Key::Action,1);
  press(c,Key::Action,2);
  auto e = c.drain();
  CHECK(e.size()==1 && c.held(Key::Action));
  lift(c,Key::Action,1);
  CHECK(c.drain().empty() && c.held(Key::Action));
  lift(c,Key::Action,2);
  CHECK(!c.held(Key::Action) && c.drain().size()==1);

  // Movement, camera and action remain independent.
  auto stick = c.stickArea();
  stickPush(c,7,0.f,-0.45f);
  CHECK(c.held(Key::Up));
  c.down(8,1400,400,T0);
  c.move(8,1430,410);
  press(c,Key::Action,9);
  e = c.drain();
  CHECK(has(e,Key::Up,Event::Down) && has(e,Key::Action,Event::Down) && has(e,Event::Look));
  c.up(8,1430,410);
  CHECK(c.held(Key::Up) && c.held(Key::Action));
  c.up(7,0,0);
  CHECK(!c.held(Key::Up) && c.held(Key::Action));
  lift(c,Key::Action,9);
  c.drain();

  // Mode changes release continuous keys and quarantine fingers until lifted.
  stickPush(c,7,0.f,-0.45f);
  press(c,Key::Action,9);
  c.drain();
  c.configure(2400,1080,Mode::Menu);
  e = c.drain();
  CHECK(has(e,Key::Up,Event::Up) && has(e,Key::Action,Event::Up));
  c.move(7,1200,200);
  c.up(7,1200,100);
  c.up(9,1200,100);
  CHECK(c.drain().empty());
  }

void keyboard() {
  Controls c;
  c.configure(2400,1080,Mode::Menu);
  c.drain();

  tap(c,special(c,Special::Keyboard).rect);
  CHECK(c.typing());

  // Keyboard emits text only, with a separate explicit OK.
  auto digit = c.buttons()[0].rect;
  tap(c,digit);
  auto e = c.drain();
  CHECK(e.size()==1 && e[0].kind==Event::Text && e[0].code=='1');

  press(c,Key::Enter);
  lift(c,Key::Enter);
  CHECK(!c.typing());
  e = c.drain();
  CHECK(e.size()==2 && e[0].key==Key::Enter);
  }

void lockpick() {
  Controls c;
  c.configure(2400,1080,Mode::Lockpick);
  c.drain();

  press(c,Key::Left);
  c.tick(T0+5000);
  CHECK(c.drain().empty());
  lift(c,Key::Left);
  auto e = c.drain();
  CHECK(e.size()==2 && e[0].key==Key::Left);
  }

void layoutFitsScreens() {
  Controls c;
  for(auto screen:screens) {
    for(auto mode:{Mode::Game, Mode::Menu, Mode::Inventory, Mode::Dialog, Mode::Lockpick, Mode::Video}) {
      c.configure(screen.w,screen.h,mode);
      c.resetPointers();
      c.drain();
      checkLayout(c,screen);
      }

    // the inventory with a second list
    c.inventoryLists(true);
    c.configure(screen.w,screen.h,Mode::Inventory);
    c.resetPointers();
    c.drain();
    checkLayout(c,screen);
    c.inventoryLists(false);
    }

  // All configurable sizes fit and retain disjoint hit targets.
  for(float size:{0.6f, 0.8f, 1.f, 1.1f, 1.2f, 1.3f, 1.4f}) {
    for(auto screen:screens) {
      for(auto mode:{Mode::Game, Mode::Menu, Mode::Dialog, Mode::Lockpick, Mode::Video}) {
        c.configure(screen.w,screen.h,mode);
        c.preferences(size,0.28f,1.f);
        c.resetPointers();
        c.drain();
        checkLayout(c,screen);
        }
      }
    }
  }

void floatingStick() {
  Controls c;
  c.configure(2400,1080,Mode::Game);
  c.resetPointers();
  c.drain();

  // Nothing is shown until a finger touches the left side
  CHECK(!c.stickActive());

  // Any free place of the left side starts the movement, the center of the stick is under the finger
  const float x = 150, y = 300;
  c.down(1,x,y,T0);
  CHECK(c.stickActive());
  CHECK(std::abs(c.stickCenterX()-x)<0.01f && std::abs(c.stickCenterY()-y)<0.01f);
  CHECK(!c.held(Key::Up));

  c.move(1,x,y-c.stickArea().w*0.45f);
  CHECK(c.held(Key::Up));
  c.up(1,x,y);
  CHECK(!c.held(Key::Up) && !c.stickActive());

  // The right side is the camera
  c.down(2,2000,300,T0);
  CHECK(!c.stickActive());
  c.up(2,2000,300);
  }

void steering() {
  Controls c;
  c.configure(2400,1080,Mode::Game);
  c.drain();

  // A small lateral deviation while walking must never strafe or turn.
  auto stick = c.stickArea();
  stickPush(c,1,-0.10f,-0.40f);
  c.tick(10000);
  c.drain();
  c.tick(10016);
  auto e = c.drain();
  CHECK(c.held(Key::Up) && !c.held(Key::StrafeLeft) && !c.held(Key::StrafeRight));
  CHECK(!has(e,Event::Look));

  c.move(1,cx(stick)-stick.w*0.4f,cy(stick)-stick.h*0.3f);
  c.tick(10032);
  e = c.drain();
  bool turns = false;
  for(const auto& ev:e) {
    if(ev.kind==Event::Look) {
      CHECK(ev.x<0);
      turns = true;
      }
    }
  CHECK(turns);

  c.resetPointers();
  c.drain();
  c.tick(10048);
  CHECK(c.drain().empty());
  }

void cameraIsResolutionIndependent() {
  // Rotation preserves subpixel motion and is identical whether Android delivers a swipe
  // in one event or in 100 events.
  auto swipe = [](float w, float h, int steps) {
    Controls model;
    model.configure(w,h,Mode::Game);
    model.drain();
    model.down(1,w*0.55f,h*0.35f,0);
    for(int i=1; i<=steps; ++i)
      model.move(1,w*(0.55f+0.05f*float(i)/float(steps)),h*0.35f);
    float yaw = 0;
    for(const auto& ev:model.drain()) {
      if(ev.kind==Event::Look)
        yaw += ev.x;
      }
    return yaw;
    };
  CHECK(std::abs(swipe(2400,1080,1)-9.f)<0.001f);
  CHECK(std::abs(swipe(1280,720,100)-9.f)<0.001f);
  CHECK(std::abs(swipe(2400,1080,100)-swipe(2400,1080,1))<0.001f);
  }

void menuGestures() {
  Controls c;
  c.configure(2400,1080,Mode::Menu);
  c.resetPointers();
  c.drain();

  // Direct menus emit a tap only for an unmoved release, never a swipe.
  c.down(1,400,400,0);
  c.up(1,400,400);
  auto e = c.drain();
  CHECK(e.size()==3 && e[0].kind==Event::MenuTouch && e[0].code==0 && e[1].code==2 && e[2].kind==Event::Tap);

  c.down(1,400,400,0);
  c.move(1,400,200);
  c.up(1,400,200);
  e = c.drain();
  CHECK(e.size()==4 && e[0].kind==Event::MenuTouch && e[1].code==1 && e[2].code==2 && e[3].kind==Event::Scroll);

  c.down(1,400,400,0);
  c.move(1,400,200);
  c.move(1,400,400);
  c.up(1,400,400);
  for(const auto& event:c.drain())
    CHECK(event.kind==Event::MenuTouch);

  // Horizontal slider gestures carry pointer phases but never activate a menu row.
  c.down(1,1400,400,0);
  c.move(1,1800,400);
  c.up(1,1800,400);
  e = c.drain();
  CHECK(e.size()==3);
  for(const auto& ev:e)
    CHECK(ev.kind==Event::MenuTouch);

  // Menu input has one gesture owner; a second finger cannot confirm another row.
  c.down(1,1400,400,0);
  c.down(2,400,500,0);
  c.up(2,400,500);
  e = c.drain();
  CHECK(e.size()==1 && e[0].kind==Event::MenuTouch && e[0].code==0);
  c.resetPointers();
  e = c.drain();
  CHECK(e.size()==1 && e[0].kind==Event::MenuTouch && e[0].code==3);
  c.up(1,1400,400);
  CHECK(c.drain().empty());
  }

void availabilityKeepsMovement() {
  Controls c;
  c.configure(2400,1080,Mode::Game);
  c.preferences(1.f,0.28f,1.f);
  c.resetPointers();
  c.drain();

  // Context changes hide controls and release them without interrupting movement.
  stickPush(c,1,0.f,-0.45f);
  press(c,Key::Action,2);
  c.drain();
  auto actionRect = button(c,Key::Action).rect;
  c.availability(~keyMask(Key::Action),false,false,true);
  CHECK(c.held(Key::Up) && !c.held(Key::Action) && !button(c,Key::Action).visible);
  c.drain();
  c.up(2,actionRect.x+1,actionRect.y+1);
  CHECK(c.drain().empty());
  c.down(3,actionRect.x+1,actionRect.y+1,0);
  CHECK(!c.held(Key::Action));
  }

void inventoryActions() {
  Controls c;
  c.configure(2400,1080,Mode::Inventory);
  c.resetPointers();
  c.drain();

  // Actions without an item are hidden and cannot activate.
  c.availability(keyMask(Key::Escape),false,false,false);
  CHECK(!button(c,Key::Weapon).visible && !button(c,Key::Weapon).enabled);
  press(c,Key::Weapon);
  lift(c,Key::Weapon);
  CHECK(!has(c.drain(),Key::Weapon,Event::Down));

  // Drop is a single action on release, never repeated.
  c.availability(~uint64_t(0),false,false,false);
  c.drain();
  press(c,Key::Weapon);
  c.tick(T0+5000);
  CHECK(c.drain().empty());
  lift(c,Key::Weapon);
  auto e = c.drain();
  CHECK(e.size()==2 && e[0].key==Key::Weapon && e[1].kind==Event::Up);

  // Categories only emit a Special event.
  for(int i=0; i<CategoryCount; ++i) {
    tap(c,special(c,Special::FirstCategory+i).rect,3);
    e = c.drain();
    CHECK(e.size()==1 && e[0].kind==Event::Special && e[0].code==uint32_t(Special::FirstCategory+i));
    }
  }

void settingsAreIsolated() {
  Controls c;
  c.configure(2400,1080,Mode::Menu);
  c.resetPointers();
  c.drain();

  tap(c,special(c,Special::Settings).rect,2);
  CHECK(c.settingsVisible());
  c.drain();

  // Settings cannot rotate the camera or fire game actions.
  c.down(3,2000,700,0);
  c.move(3,2100,700);
  CHECK(!has(c.drain(),Event::Look));
  }

void resetStress() {
  // No held controls survive a reset, whatever the mode transitions in between.
  Controls c;
  std::mt19937 rng(1337);
  for(int n=0; n<2000; ++n) {
    c.configure(2400,1080,n%2 ? Mode::Game : Mode::Menu);
    for(int i=0; i<8; ++i)
      c.down(i,float(rng()%2400),float(rng()%1080),T0);
    for(int i=0; i<8; ++i)
      c.move(i,float(rng()%2400),float(rng()%1080));
    c.resetPointers();
    for(int k=0; k<int(Key::Count); ++k)
      CHECK(!c.held(Key(k)));
    c.drain();
    }
  }

}

int main() {
  menuSwipeDoesNotConfirm();
  explicitConfirmation();
  gameMultitouch();
  keyboard();
  lockpick();
  layoutFitsScreens();
  floatingStick();
  steering();
  cameraIsResolutionIndependent();
  menuGestures();
  availabilityKeepsMovement();
  inventoryActions();
  settingsAreIsolated();
  resetStress();
  std::puts("PASS");
  return 0;
  }
