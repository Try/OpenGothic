#pragma once

#include "utils/gamepadinput.h"

#include <deque>

// Menu commands can open synchronous dialogs. Keep them out of native frame
// callbacks, without retaining pointers to a menu, script VM or dialog.
class GamepadMenuInput final {
  public:
    static constexpr size_t MaxCommands = 128;

    void push(const GamepadInput::Command& command, GamepadInput::Screen screen,
              Tempest::Gamepad::DeviceId device) {
      if(overflow || command.type!=GamepadInput::Command::Type::Navigate ||
         screen.context!=GamepadInput::Context::Menu || device==0)
        return;
      if(pending.size()==MaxCommands) {
        pending.clear();
        overflow = true;
        return;
        }
      pending.push_back({command.action,screen,device});
      }

    GamepadInput::Command pop(GamepadInput::Screen screen, Tempest::Gamepad::DeviceId device) {
      while(!pending.empty()) {
        const auto next = pending.front();
        pending.pop_front();
        if(next.screen==screen && next.device==device)
          return {GamepadInput::Command::Type::Navigate,next.action};
        }
      return {};
      }

    void clear() {
      pending.clear();
      overflow = false;
      }

    bool empty() const { return pending.empty(); }
    bool overflowed() const { return overflow; }

  private:
    struct Pending {
      KeyCodec::Action          action;
      GamepadInput::Screen      screen;
      Tempest::Gamepad::DeviceId device;
      };

    std::deque<Pending> pending;
    bool                overflow = false;
  };
