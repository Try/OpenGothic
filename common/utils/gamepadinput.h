#pragma once

#include <Tempest/Gamepad>
#include <array>

#include "gamepadsettings.h"

// Pure routing state. MainWindow owns the device queue and delivers commands.
class GamepadInput final {
  public:
    using Pad = Tempest::Gamepad;

    enum class Context : uint8_t {
      Blocked, Gameplay, Cutscene, Menu, Dialog, Inventory, Document, Chapter, Video
      };

    struct Screen {
      Context  context = Context::Blocked;
      uint64_t revision = 0;

      bool operator==(const Screen& other) const {
        return context==other.context && revision==other.revision;
        }
      };

    struct Command {
      enum class Type : uint8_t { None, Press, Release, Navigate, Cancel, CancelAll };
      Type             type = Type::None;
      KeyCodec::Action action = KeyCodec::Idle;
      KeyCodec::Mapping mapping = KeyCodec::Mapping::Primary;
      };

    struct Look {
      float yaw = 0;
      float pitch = 0;
      };

    Pad::DeviceId device() const { return activeDevice; }
    Screen        screen() const { return current; }
    void          selectDevice(Pad::DeviceId id, const Pad::State& state);
    Command       setContext(Screen screen, const Pad::State& state);
    const GamepadSettings& settings() const { return config; }
    Command       configure(GamepadSettings next, const Pad::State& state);
    void          reset(const Pad::State& state);
    Command       process(const Pad::Event& event, uint64_t now);
    Command       update(const Pad::State& state, uint64_t now);
    std::array<Command, 4> movement();
    Look          look(uint64_t dt) const;

  private:
    KeyCodec::Action        gameplayAction(Pad::Button button) const;
    KeyCodec::Action        uiAction(Pad::Button button) const;
    static int             axisDirection(float value, int previous, float release);
    Command                navigation(uint64_t now);
    bool                   navigates() const;
    bool                   actionDelivered(KeyCodec::Action action) const;

    GamepadSettings config;
    Pad::DeviceId activeDevice = 0;
    Screen        current;
    uint32_t      buttons = 0;
    uint32_t      blockedButtons = 0;
    uint32_t      delivered = 0;
    bool          blockedStick = false;
    bool          blockedLook = false;
    int           stickX = 0;
    int           stickY = 0;
    int           moveX = 0;
    int           moveY = 0;
    float         lookX = 0;
    float         lookY = 0;
    KeyCodec::Action direction = KeyCodec::Idle;
    uint64_t      nextRepeat = 0;
  };
