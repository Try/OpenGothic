#include "mobilecontrolsmodel.h"

#include <algorithm>
#include <cmath>
#include <utility>

using namespace Mobile;

namespace {
constexpr size_t   MaxFingers     = 16;
constexpr uint64_t RepeatDelay    = 400;
constexpr uint64_t RepeatInterval = 120;

// Distances are in layout units, multiplied by Controls::unit()
constexpr float    TapSlop        = 22.f;
constexpr float    SwipeDistance  = 42.f;

constexpr float    CameraYawSpan   = 180.f;
constexpr float    CameraPitchSpan = 100.f;
constexpr float    StickTurnRate   = 100.f;
constexpr float    StickPressAt    = 0.30f;
constexpr float    StickReleaseAt  = 0.20f;

constexpr float    MenuGestureSplit = 0.42f;
}

void Controls::acquire(Key key) {
  if(refs[size_t(key)]++==0)
    events.push_back({Event::Down,key});
  }

void Controls::release(Key key) {
  auto& n = refs[size_t(key)];
  if(n>0 && --n==0)
    events.push_back({Event::Up,key});
  }

void Controls::pulse(Key key) {
  events.push_back({Event::Down,key});
  events.push_back({Event::Up,key});
  }

void Controls::physical(Key key, bool on) {
  if(physicalHeld[size_t(key)]==on)
    return;
  physicalHeld[size_t(key)] = on;
  if(on)
    acquire(key); else
    release(key);
  }

void Controls::resetPointers() {
  cancel();
  fingers.clear();
  }

void Controls::preferences(float buttons, float zone, float sensitivity) {
  buttons     = std::clamp(buttons,0.6f,1.4f);
  zone        = std::clamp(zone,0.15f,0.4f);
  sensitivity = std::clamp(sensitivity,0.4f,2.f);
  if(buttonScale==buttons && deadzone==zone && lookSensitivity==sensitivity)
    return;

  cancel();
  buttonScale     = buttons;
  deadzone        = zone;
  lookSensitivity = sensitivity;
  rebuild();
  }

void Controls::inventoryLists(bool two) {
  if(twoLists==two)
    return;
  twoLists = two;
  if(current==Mode::Inventory) {
    cancel();
    rebuild();
    }
  }

void Controls::gui(float scale) {
  scale = std::clamp(scale,0.4f,2.5f);
  if(guiScale==scale)
    return;
  guiScale = scale;
  if(current==Mode::Inventory) {
    cancel();
    rebuild();
    }
  }

void Controls::dialogPad(bool visible) {
  if(padVisible==visible)
    return;
  padVisible = visible;
  if(current==Mode::Dialog) {
    cancel();
    rebuild();
    }
  }

InventoryLayout Mobile::inventoryLayout(float W, float H, bool twoLists, float gui) {
  InventoryLayout l;
  const float cols = float(InventoryLayout::Columns);
  const float rows = float(InventoryLayout::Rows);

  // q is the size of the interface compared to the largest one that fits the screen
  const float q = std::min(1.f,0.766f*gui);
  l.side = std::min(0.128f*H*q,0.095f*W);
  const float side   = l.side;
  const float top    = 0.125f*H;
  const float tabW   = 0.9f*side;
  const float tabsX  = W-0.03f*W-tabW;
  const float inset  = 0.034f*W;

  const float tabGap = 0.12f*side;
  l.list[1] = {tabsX-tabGap-cols*side,top,cols*side,rows*side};
  l.list[0] = {inset,top,cols*side,rows*side};

  // close button over the tabs, the title of the player's list ends before it
  const float closeW = 0.57f*side;
  l.close = {tabsX+(tabW-closeW)/2,0.046f*H,closeW,closeW};
  l.header[1] = {l.list[1].x,0.053f*H,l.close.x-0.05f*side-l.list[1].x,0.058f*H*q};
  l.header[0] = {l.list[0].x,0.053f*H,cols*side,0.058f*H*q};

  const float tabTop    = top+0.042f*H*q;
  const float tabBottom = top+rows*side-0.042f*H*q;
  const float pitch     = (tabBottom-tabTop)/float(CategoryCount);
  for(int i=0; i<CategoryCount; ++i)
    l.tab[i] = {tabsX,tabTop+float(i)*pitch,tabW,pitch*0.9f};

  const float gap = 0.03f*W;
  const float x0  = twoLists ? l.list[0].x+l.list[0].w+gap : inset;
  const float x1  = twoLists ? l.list[1].x-gap : l.list[1].x-0.054f*W;
  l.info = {x0,top,std::max(0.f,x1-x0),0.5f*H*q};

  const float btn = 0.07f*H*q;
  const float by  = l.info.y+l.info.h-0.02f*H*q-btn;
  const float pad = 0.008f*W;
  l.use   = {l.info.x+pad,by,btn,btn};
  l.drop  = {l.info.x+l.info.w-pad-btn,by,btn,btn};
  l.stack = {l.drop.x-pad-btn,by,btn,btn};
  return l;
  }

void Controls::configure(float w, float h, Mode next) {
  if(w==width && h==height && next==current && !layout.empty())
    return;

  cancel();
  width        = w;
  height       = h;
  current      = next;
  keyboard     = false;
  settingsOpen = false;
  scale        = std::min(width/1000.f,height/440.f);
  rebuild();
  }

Button& Controls::add(float x, float y, float w, float h, const char* label, Key key) {
  Button b;
  b.rect  = {x,y,w*scale,h*scale};
  b.label = label;
  b.key   = key;
  layout.push_back(std::move(b));
  return layout.back();
  }

Button& Controls::round(float x, float y, float diameter, const char* label, Key key) {
  const float size = diameter*sizeFactor();
  return add(x-size*scale/2,y-size*scale/2,size,size,label,key);
  }

void Controls::rebuild() {
  layout.clear();
  stick = {};
  if(current==Mode::Loading)
    return;

  const float l = 16.f*scale;
  const Anchor a = {l, width-l, 12.f*scale, height-12.f*scale};

  if(settingsOpen)
    buildSettings(a); else
  if(keyboard)
    buildKeyboard(a); else {
    switch(current) {
      case Mode::Video:
        round(fromRight(70),fromBottom(70),60,"SKIP",Key::Escape);
        break;
      case Mode::Lockpick:
        buildLockpick(a);
        break;
      case Mode::Game:
        buildGame(a);
        break;
      case Mode::Menu:
        buildMenu(a);
        break;
      case Mode::Inventory:
        buildInventory();
        break;
      case Mode::Dialog:
        buildDialog(a);
        break;
      case Mode::Loading:
        break;
      }
    }
  }

void Controls::buildSettings(const Anchor& a) {
  static const char* labels[SettingCount] = {
    "Button size", "3D resolution", "Shadow quality", "SSAO effects",
    "FPS limit", "Joystick deadzone", "Camera sensitivity", "Vibration", "GUI scale"
    };
  for(int i=0; i<SettingCount; ++i)
    add(width*0.22f,a.t+float(i)*44*scale,560,40,labels[i],Key::Enter).special = Special::FirstSetting+i;
  add(a.r-64*scale,a.b-60*scale,64,60,"BACK",Key::Escape).special = Special::Settings;
  }

void Controls::buildKeyboard(const Anchor& a) {
  static const char* rows[] = {"1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm"};
  const float kw = std::min((width-2*a.l)/11.f,72.f*scale);
  const float kh = 52.f*scale;
  float       y  = a.b-4*kh-10*scale;
  for(const char* row:rows) {
    const size_t n = std::string(row).size();
    float        x = (width-float(n)*kw)/2;
    for(size_t i=0; i<n; ++i) {
      const char text[2] = {row[i],'\0'};
      add(x,y,kw/scale-4,48,text,Key::Enter).character = uint32_t(row[i]);
      x += kw;
      }
    y += kh;
    }
  add(a.l,a.t,100,54,"HIDE",Key::Enter).special = Special::Keyboard;
  add(a.l+110*scale,a.t,110,54,"SPACE",Key::Enter).character = ' ';
  add(a.r-220*scale,a.t,100,54,"DEL",Key::Backspace).repeat = true;
  add(a.r-110*scale,a.t,110,54,"OK",Key::Enter);
  }

void Controls::buildLockpick(const Anchor&) {
  round(fromLeft(70),  fromBottom(70),66,"LEFT",  Key::Left);
  round(fromLeft(150), fromBottom(70),66,"RIGHT", Key::Right);
  round(fromRight(70), fromBottom(70),66,"CANCEL",Key::Down);
  }

void Controls::buildGame(const Anchor&) {
  const float s = scale;
  const float k = sizeFactor();
  auto left   = [&](float v) { return fromLeft(v); };
  auto right  = [&](float v) { return fromRight(v); };
  auto top    = [&](float v) { return fromTop(v); };
  auto bottom = [&](float v) { return fromBottom(v); };

  // Diameter d is given for the default button size
  auto icon = [&](float x, float y, float d, const char* label, Key key, bool hold=false, int special=0) {
    auto& b = round(x,y,d,label,key);
    b.hold    = hold;
    b.special = special;
    };

  icon(left(61),   top(36), 50, "MENU",      Key::Escape);
  icon(left(122),  top(36), 50, "CHARACTER", Key::Status);
  icon(right(124), top(36), 50, "LOG",       Key::Log);
  icon(right(62),  top(36), 50, "MAP",       Key::Map);

  const float stickSize = 180*k*s;
  stick = {left(153.5f)-stickSize/2,bottom(166.5f)-stickSize/2,stickSize,stickSize};
  icon(left(61),  bottom(36.5f), 50, "SNEAK", Key::Sneak);
  icon(left(123), bottom(36.5f), 50, "WALK",  Key::Walk);

  icon(right(198),   bottom(221.5f), 88, "ACTION", Key::Action, true);
  icon(right(285),   bottom(166.5f), 68, "< HIT",  Key::AttackLeft, true);
  icon(right(111),   bottom(166.5f), 68, "HIT >",  Key::AttackRight, true);
  icon(right(198),   bottom(111),    88, "BLOCK",  Key::Block, true);
  icon(right(72),    bottom(266.5f), 66, "JUMP",   Key::Jump, true);
  icon(right(62),    bottom(36.5f),  50, "INVENTORY", Key::Inventory);
  icon(right(372),   bottom(230),    56, "TARGET", Key::Enter, false, Special::Target);
  icon(right(372),   bottom(150),    56, "SPELLS", Key::Enter, false, Special::Spell);
  icon(right(300),   bottom(276),    52, "WEAPON", Key::Weapon);

  // The hotbar is centered below the status bars and does not depend on the button size
  for(int i=0; i<10; ++i) {
    const std::string label = std::to_string((i+1)%10);
    // slots touch each other, the half unit gap only protects from rounding
    add(width/2-HotbarSlot*5*s+float(i)*HotbarSlot*s+0.25f*s,height-(HotbarBottom+HotbarSlot)*s,
        HotbarSlot-0.5f,HotbarSlot-0.5f,label.c_str(),Key(int(Key::One)+i));
    }
  applyAvailability();
  }

void Controls::buildMenu(const Anchor&) {
  round(fromRight(62), fromTop(36),     50,"SETTINGS",Key::Enter).special = Special::Settings;
  round(fromRight(70), fromBottom(70),  66,"BACK",    Key::Escape);
  round(fromRight(150),fromBottom(70),  66,"KEYBOARD",Key::Enter).special = Special::Keyboard;
  }

void Controls::buildInventory() {
  const auto l = inventoryLayout(width,height,twoLists,guiScale);
  auto tile = [&](const Rect& r, const char* label, Key key, int special=0) {
    Button b;
    b.rect    = r;
    b.label   = label;
    b.key     = key;
    b.special = special;
    layout.push_back(std::move(b));
    };

  static const char* categories[CategoryCount] = {
    "ALL", "WEAPONS", "ARMOR", "JEWELRY", "MAGIC", "FOOD", "POTIONS", "DOCUMENTS", "OTHER"
    };
  tile(l.close,"CLOSE",Key::Escape);
  for(int i=0; i<CategoryCount; ++i)
    tile(l.tab[i],categories[i],Key::Enter,Special::FirstCategory+i);

  // The lists are scrolled with a finger, so there are no arrows
  tile(l.use,  "USE",  Key::Enter);
  tile(l.drop, "DROP", Key::Weapon);
  tile(l.stack,"STACK",Key::Jump);
  applyAvailability();
  }

void Controls::buildDialog(const Anchor&) {
  round(fromLeft(61),fromTop(36),50,"MENU",Key::Escape);

  // leaves the phrase or the conversation
  round(fromRight(70),fromBottom(70),66,"BACK",Key::Escape);

  // answers are tapped, the cross is a spare way to choose them
  if(padVisible) {
    const float cx  = fromLeft(100);
    const float cy  = fromBottom(100);
    const float gap = 62*sizeFactor()*scale;
    round(cx,    cy-gap,56,"UP",   Key::Up).repeat    = true;
    round(cx,    cy+gap,56,"DOWN", Key::Down).repeat  = true;
    round(cx-gap,cy,    56,"<",    Key::Left).repeat  = true;
    round(cx+gap,cy,    56,">",    Key::Right).repeat = true;
    round(fromRight(170),fromBottom(70),66,"OK",Key::Enter);
    }
  }

bool Controls::hasOwner(bool Finger::* role) const {
  for(const auto& p:fingers) {
    if(p.second.*role && !p.second.blocked)
      return true;
    }
  return false;
  }

void Controls::down(int id, float x, float y, uint64_t now) {
  if(fingers.find(id)!=fingers.end() || fingers.size()>=MaxFingers)
    return;

  Finger f;
  f.startX = f.x = x;
  f.startY = f.y = y;
  for(size_t i=0; i<layout.size(); ++i) {
    if(layout[i].visible && layout[i].enabled && layout[i].rect.contains(x,y)) {
      f.button = int(i);
      break;
      }
    }

  const bool game = current==Mode::Game && !settingsOpen;
  if(f.button>=0) {
    const auto& button = layout[size_t(f.button)];
    if(button.hold) {
      acquire(button.key);
      } else
    if(button.repeat) {
      pulse(button.key);
      f.repeatAt = now+RepeatDelay;
      }
    } else
  if(game && availableMovement && x<width*MenuGestureSplit) {
    // the left part of the screen is the joystick, it appears under the finger
    if(!hasOwner(&Finger::joystick)) {
      f.joystick = true;
      f.originX  = x;
      f.originY  = y;
      joyOx      = x;
      joyOy      = y;
      joyActive  = true;
      joystick(x,y,x,y);
      }
    } else
  if(game && x>width*MenuGestureSplit) {
    f.camera = !hasOwner(&Finger::camera);
    } else
  if(current==Mode::Menu || current==Mode::Inventory || current==Mode::Dialog) {
    f.swipe = true;
    }

  if(f.swipe && (current==Mode::Menu || current==Mode::Inventory || current==Mode::Dialog) && !keyboard) {
    // Menu input has a single gesture owner
    if(hasOwner(&Finger::swipe))
      return;
    events.push_back({Event::MenuTouch,Key::Enter,x,y,0});
    }
  fingers.emplace(id,f);
  }

void Controls::joystick(float x, float y, float ox, float oy) {
  const float radius = stick.w/2;
  joyX = (x-ox)/radius;
  joyY = (y-oy)/radius;
  const float len = std::sqrt(joyX*joyX+joyY*joyY);
  if(len>1) {
    joyX /= len;
    joyY /= len;
    }

  // Forward/back use hysteresis; horizontal displacement steers continuously in tick().
  // A generous central cone prevents accidental turning while walking straight.
  const float values[2] = {-joyY,joyY};
  const Key   keys  [2] = {Key::Up,Key::Down};
  for(size_t i=0; i<2; ++i) {
    const bool on = values[i]>(joyHeld[i] ? StickReleaseAt : StickPressAt);
    if(on==joyHeld[i])
      continue;
    joyHeld[i] = on;
    if(on)
      acquire(keys[i]); else
      release(keys[i]);
    }
  }

void Controls::move(int id, float x, float y) {
  auto it = fingers.find(id);
  if(it==fingers.end() || it->second.blocked)
    return;

  auto& f = it->second;
  if(f.swipe && (current==Mode::Menu || current==Mode::Inventory || current==Mode::Dialog) && !keyboard)
    events.push_back({Event::MenuTouch,Key::Enter,x,y,1});
  if(f.swipe && std::hypot(x-f.startX,y-f.startY)>TapSlop*scale)
    f.cancelled = true;
  if(f.joystick)
    joystick(x,y,f.originX,f.originY);
  if(f.camera) {
    const float yaw   = (x-f.x)/width *CameraYawSpan  *lookSensitivity;
    const float pitch = (y-f.y)/height*CameraPitchSpan*lookSensitivity;
    events.push_back({Event::Look,Key::Enter,yaw,pitch});
    }
  if(f.button>=0) {
    auto& button = layout[size_t(f.button)];
    if(!button.hold && (!button.rect.contains(x,y) || std::hypot(x-f.startX,y-f.startY)>TapSlop*scale))
      f.cancelled = true;
    }
  f.x = x;
  f.y = y;
  }

void Controls::cancelFinger(Finger& f) {
  if(f.blocked)
    return;
  if(f.button>=0 && layout[size_t(f.button)].hold)
    release(layout[size_t(f.button)].key);
  if(f.joystick) {
    const Key keys[2] = {Key::Up,Key::Down};
    for(size_t i=0; i<2; ++i) {
      if(joyHeld[i])
        release(keys[i]);
      joyHeld[i] = false;
      }
    joyX = joyY = 0;
    joyActive = false;
    }
  f.blocked = true;
  }

void Controls::activate(Button button) {
  if(button.special==Special::Keyboard) {
    cancel();
    keyboard = !keyboard;
    rebuild();
    return;
    }
  if(button.special==Special::Settings) {
    cancel();
    settingsOpen = !settingsOpen;
    rebuild();
    events.push_back({Event::Special,Key::Enter,0,0,uint32_t(button.special)});
    return;
    }
  if(button.special!=Special::None) {
    events.push_back({Event::Special,Key::Enter,0,0,uint32_t(button.special)});
    return;
    }
  if(button.character!=0) {
    events.push_back({Event::Text,Key::Enter,0,0,button.character});
    return;
    }
  if(keyboard && button.key==Key::Enter) {
    cancel();
    keyboard = false;
    rebuild();
    }
  pulse(button.key);
  }

void Controls::up(int id, float x, float y) {
  auto it = fingers.find(id);
  if(it==fingers.end())
    return;

  const Finger f = it->second;
  cancelFinger(it->second);
  fingers.erase(it);
  if(f.blocked)
    return;

  const float slop = TapSlop*scale;
  if(f.button>=0) {
    // Buttons confirm on release, only when the finger has not wandered off
    const Button button = layout[size_t(f.button)];
    if(button.hold || button.repeat || f.cancelled || !button.rect.contains(x,y) ||
       std::hypot(x-f.startX,y-f.startY)>slop)
      return;
    activate(button);
    return;
    }

  if(!f.swipe || keyboard || settingsOpen)
    return;

  const float dx = x-f.startX;
  const float dy = y-f.startY;
  const bool  vertical = std::abs(dy)>SwipeDistance*scale && std::abs(dy)>std::abs(dx)*1.25f;
  if(current==Mode::Menu || current==Mode::Inventory) {
    events.push_back({Event::MenuTouch,Key::Enter,x,y,2});
    // The inventory list follows the finger, see InventoryMenu::mobileTouch
    if(vertical && current==Mode::Menu)
      events.push_back({Event::Scroll,Key::Enter,0,dy}); else
    if(!f.cancelled && std::hypot(dx,dy)<=slop)
      events.push_back({Event::Tap,Key::Enter,x,y});
    return;
    }
  if(current==Mode::Dialog) {
    events.push_back({Event::MenuTouch,Key::Enter,x,y,2});
    if(!vertical && !f.cancelled && std::hypot(dx,dy)<=slop)
      events.push_back({Event::Tap,Key::Enter,x,y}); else
    if(vertical && padVisible)
      pulse(dy<0 ? Key::Up : Key::Down);
    return;
    }
  if(vertical)
    pulse(dy<0 ? Key::Up : Key::Down);
  }

void Controls::cancel() {
  events.clear();
  if((current==Mode::Menu || current==Mode::Inventory || current==Mode::Dialog) && hasOwner(&Finger::swipe))
    events.push_back({Event::MenuTouch,Key::Enter,0,0,3});

  // Release only held controls; cancelling must never commit a tap action.
  for(size_t i=0; i<refs.size(); ++i) {
    if(refs[i])
      events.push_back({Event::Up,Key(i)});
    }
  refs.fill(0);
  physicalHeld.fill(false);
  joyHeld.fill(false);
  joyX = joyY = 0;
  joyActive = false;
  for(auto& p:fingers)
    p.second.blocked = true;
  }

void Controls::tick(uint64_t now) {
  const float dt = lastTick && now>=lastTick ? float(std::min<uint64_t>(now-lastTick,100))/1000.f : 0.f;
  lastTick = now;

  if(current==Mode::Game && std::abs(joyX)>deadzone) {
    const float turn = std::copysign((std::abs(joyX)-deadzone)/(1.f-deadzone),joyX);
    events.push_back({Event::Look,Key::Enter,turn*StickTurnRate*dt,0});
    }

  for(auto& p:fingers) {
    auto& f = p.second;
    if(f.blocked || f.cancelled || f.button<0 || f.repeatAt==0 || now<f.repeatAt)
      continue;
    pulse(layout[size_t(f.button)].key);
    f.repeatAt = now+RepeatInterval;
    }
  }

std::vector<Event> Controls::drain() {
  std::vector<Event> ret;
  ret.swap(events);
  return ret;
  }

bool Controls::pressed(size_t index) const {
  for(const auto& p:fingers) {
    if(!p.second.blocked && !p.second.cancelled && p.second.button==int(index))
      return true;
    }
  return false;
  }

void Controls::applyAvailability() {
  if((current!=Mode::Game && current!=Mode::Inventory) || settingsOpen || keyboard)
    return;

  for(auto& b:layout) {
    bool usable = (availableKeys & keyMask(b.key))!=0;
    if(b.special>=Special::FirstCategory)
      usable = true; else
    if(b.special==Special::Target)
      usable = availableTarget; else
    if(b.special==Special::Spell)
      usable = availableSpells;

    // The hotbar stays visible while the player can move, other controls hide when unusable
    // Inventory actions disappear when there is no item to act on, the tabs and the close button stay.
    const bool hotbar  = b.key>=Key::One && b.key<=Key::Zero && availableMovement;
    const bool pinned  = current==Mode::Inventory && (b.special>=Special::FirstCategory || b.key==Key::Escape);
    b.visible = (pinned || hotbar) ? true : usable;
    b.enabled = usable;
    }
  }

void Controls::availability(uint64_t keys, bool target, bool spells, bool movement) {
  availableKeys     = keys;
  availableTarget   = target;
  availableSpells   = spells;
  availableMovement = movement;
  applyAvailability();

  // Release only controls that disappeared; never reset a walking finger on focus changes.
  for(auto& pair:fingers) {
    auto& f = pair.second;
    const bool hidden = f.button>=0 && (!layout[size_t(f.button)].visible || !layout[size_t(f.button)].enabled);
    if(hidden || (f.joystick && !movement))
      cancelFinger(f);
    }
  }
