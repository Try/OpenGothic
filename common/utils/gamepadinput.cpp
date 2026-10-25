#include "gamepadinput.h"

#include <algorithm>
#include <cmath>

namespace {
using Button = Tempest::Gamepad::Button;
constexpr uint32_t bit(Button button) { return uint32_t(1) << uint8_t(button); }
}

void GamepadInput::selectDevice(Pad::DeviceId id, const Pad::State& state) {
  activeDevice = id;
  reset(state);
  }

GamepadInput::Command GamepadInput::setContext(Screen screen, const Pad::State& state) {
  if(current==screen)
    return {};
  current = screen;
  reset(state);
  // Fresh keyboard input can arrive after closing UI but before the next
  // frame. Entering UI already cancelled the old gameplay holds.
  return {screen.context==Context::Gameplay ? Command::Type::Cancel : Command::Type::CancelAll};
  }

GamepadInput::Command GamepadInput::configure(GamepadSettings next, const Pad::State& state) {
  next.normalize();
  if(next==config)
    return {};
  config = next;
  reset(state);
  return {Command::Type::Cancel};
  }

void GamepadInput::reset(const Pad::State& state) {
  const float moveDeadZone = current.context==Context::Gameplay ? float(config.moveDeadZone)/100.f : .35f;
  const float lookDeadZone = float(config.lookDeadZone)/100.f;
  buttons = state.buttons;
  blockedButtons = state.buttons;
  delivered = 0;
  stickX = 0;
  stickY = 0;
  moveX = 0;
  moveY = 0;
  lookX = 0;
  lookY = 0;
  blockedStick = std::abs(state.axes[size_t(Pad::Axis::LeftX)])>moveDeadZone ||
                 std::abs(state.axes[size_t(Pad::Axis::LeftY)])>moveDeadZone;
  blockedLook = std::hypot(state.axes[size_t(Pad::Axis::RightX)],
                           state.axes[size_t(Pad::Axis::RightY)])>lookDeadZone;
  direction = KeyCodec::Idle;
  nextRepeat = 0;
  }

KeyCodec::Action GamepadInput::gameplayAction(Pad::Button button) const {
  return button<Button::Count ? config.bindings[size_t(button)] : KeyCodec::Idle;
  }

KeyCodec::Action GamepadInput::uiAction(Pad::Button button) const {
  if(current.context==Context::Menu && button==Button::West)
    return KeyCodec::UiSettings;
  if(current.context==Context::Inventory) {
    switch(button) {
      case Button::LeftShoulder:  return KeyCodec::UiPrevious;
      case Button::RightShoulder: return KeyCodec::UiNext;
      case Button::West:          return KeyCodec::UiAmount;
      case Button::North:         return KeyCodec::UiAssign;
      default: break;
      }
    }
  switch(button) {
    case Button::DpadUp:    return KeyCodec::Forward;
    case Button::DpadDown:  return KeyCodec::Back;
    case Button::DpadLeft:  return KeyCodec::Left;
    case Button::DpadRight: return KeyCodec::Right;
    case Button::South:    return KeyCodec::ActionGeneric;
    case Button::East:
    case Button::Menu:     return KeyCodec::Escape;
    default:              return KeyCodec::Idle;
    }
  }

bool GamepadInput::navigates() const {
  return current.context==Context::Menu || current.context==Context::Dialog ||
         current.context==Context::Inventory;
  }

bool GamepadInput::actionDelivered(KeyCodec::Action action) const {
  for(uint8_t b=0; b<uint8_t(Button::Count); ++b)
    if((delivered & bit(Button(b)))!=0 && gameplayAction(Button(b))==action)
      return true;
  return false;
  }

GamepadInput::Command GamepadInput::process(const Pad::Event& event, uint64_t now) {
  using Type = Pad::Event::Type;
  if(event.type==Type::Reset && (event.device==0 || event.device==activeDevice)) {
    reset({});
    return {Command::Type::Cancel};
    }
  if(activeDevice==0 || event.device!=activeDevice)
    return {};
  if(event.type==Type::Disconnected) {
    selectDevice(0, {});
    return {Command::Type::Cancel};
    }
  if((event.type!=Type::ButtonDown && event.type!=Type::ButtonUp) || event.button>=Button::Count)
    return {};

  const auto mask = bit(event.button);
  const bool down = event.type==Type::ButtonDown;
  if(down) {
    if((buttons & mask)!=0)
      return {};
    buttons |= mask;
    } else {
    buttons &= ~mask;
    blockedButtons &= ~mask;
    }

  if(current.context==Context::Blocked || (blockedButtons & mask)!=0)
    return {};
  if(current.context==Context::Gameplay || current.context==Context::Cutscene) {
    auto action = gameplayAction(event.button);
    if(action==KeyCodec::Idle)
      return {};
    if(current.context==Context::Cutscene && action!=KeyCodec::Escape &&
       action!=KeyCodec::Inventory && action!=KeyCodec::Log && action!=KeyCodec::Status)
      return {};
    if(down) {
      const bool shared = actionDelivered(action);
      delivered |= mask;
      if(shared)
        return {};
      return {Command::Type::Press, action};
      }
    if((delivered & mask)==0)
      return {};
    delivered &= ~mask;
    if(actionDelivered(action))
      return {};
    return {Command::Type::Release, action};
    }

  auto action = uiAction(event.button);
  if(action==KeyCodec::Forward || action==KeyCodec::Back || action==KeyCodec::Left || action==KeyCodec::Right)
    return navigation(now);
  if(!down || action==KeyCodec::Idle)
    return {};
  if(navigates() || action==KeyCodec::Escape)
    return {Command::Type::Navigate, action};
  return {};
  }

int GamepadInput::axisDirection(float value, int previous, float release) {
  const float press = release+.20f;
  if(!std::isfinite(value))
    return 0;
  if(value>=press || (previous>0 && value>release))
    return 1;
  if(value<=-press || (previous<0 && value<-release))
    return -1;
  return 0;
  }

GamepadInput::Command GamepadInput::update(const Pad::State& state, uint64_t now) {
  if(activeDevice==0)
    return {};
  // Queue edges have already been consumed. A neutral snapshot also rearms
  // controls whose release was discarded at a context boundary.
  blockedButtons &= state.buttons;
  const auto x = state.axes[size_t(Pad::Axis::LeftX)];
  const auto y = state.axes[size_t(Pad::Axis::LeftY)];
  const float release = current.context==Context::Gameplay ? float(config.moveDeadZone)/100.f : .35f;
  if(blockedStick && std::abs(x)<=release && std::abs(y)<=release)
    blockedStick = false;
  if(blockedStick || (!navigates() && current.context!=Context::Gameplay)) {
    stickX = 0;
    stickY = 0;
    } else {
    stickX = axisDirection(x, stickX, release);
    stickY = axisDirection(y, stickY, release);
    }

  lookX = 0;
  lookY = 0;
  const auto rx = state.axes[size_t(Pad::Axis::RightX)];
  const auto ry = state.axes[size_t(Pad::Axis::RightY)];
  if(std::isfinite(rx) && std::isfinite(ry)) {
    const float lookDeadZone = float(config.lookDeadZone)/100.f;
    const auto length = std::hypot(rx, ry);
    if(length<=lookDeadZone)
      blockedLook = false;
    else if(!blockedLook && current.context==Context::Gameplay) {
      const float scale = (std::min(length, 1.f)-lookDeadZone)/((1.f-lookDeadZone)*length);
      lookX = rx*scale;
      lookY = ry*scale;
      }
    }
  return navigation(now);
  }

std::array<GamepadInput::Command, 4> GamepadInput::movement() {
  std::array<Command, 4> commands = {};
  if(activeDevice==0 || current.context!=Context::Gameplay)
    return commands;
  size_t count = 0;
  const auto xAction = [](int x) { return x>0 ? KeyCodec::Right : KeyCodec::Left; };
  const auto yAction = [](int y) { return y>0 ? KeyCodec::Forward : KeyCodec::Back; };
  const auto add = [&commands, &count](Command::Type type, KeyCodec::Action action) {
    commands[count++] = {type, action, KeyCodec::Mapping::Secondary};
    };
  // Release both old directions before pressing new ones, including a diagonal
  // crossing. Each deflection is a physical hold, not a command replay per tick.
  if(moveX!=stickX && moveX!=0)
    add(Command::Type::Release, xAction(moveX));
  if(moveY!=stickY && moveY!=0)
    add(Command::Type::Release, yAction(moveY));
  if(moveX!=stickX && stickX!=0)
    add(Command::Type::Press, xAction(stickX));
  if(moveY!=stickY && stickY!=0)
    add(Command::Type::Press, yAction(stickY));
  moveX = stickX;
  moveY = stickY;
  return commands;
  }

GamepadInput::Look GamepadInput::look(uint64_t dt) const {
  if(activeDevice==0 || current.context!=Context::Gameplay)
    return {};
  const float seconds = float(std::min<uint64_t>(dt, 50))/1000.f;
  return {-lookX*180.f*seconds*float(config.sensitivityX)/100.f,
          lookY*120.f*seconds*float(config.sensitivityY)/100.f*(config.invertY ? -1.f : 1.f)};
  }

GamepadInput::Command GamepadInput::navigation(uint64_t now) {
  if(!navigates())
    return {};
  const auto held = buttons & ~blockedButtons;
  const int up    = ((held & bit(Button::DpadUp))!=0 || stickY>0) ? 1 : 0;
  const int down  = ((held & bit(Button::DpadDown))!=0 || stickY<0) ? 1 : 0;
  const int left  = ((held & bit(Button::DpadLeft))!=0 || stickX<0) ? 1 : 0;
  const int right = ((held & bit(Button::DpadRight))!=0 || stickX>0) ? 1 : 0;
  auto next = KeyCodec::Idle;
  if(up!=down)
    next = up ? KeyCodec::Forward : KeyCodec::Back;
  else if(left!=right)
    next = left ? KeyCodec::Left : KeyCodec::Right;

  if(next!=direction) {
    direction = next;
    nextRepeat = now+400;
    if(next!=KeyCodec::Idle)
      return {Command::Type::Navigate, next};
    }
  else if(next!=KeyCodec::Idle && now>=nextRepeat) {
    // Never replay a burst of missed repetitions after a stalled frame.
    nextRepeat = now+120;
    return {Command::Type::Navigate, next};
    }
  return {};
  }
