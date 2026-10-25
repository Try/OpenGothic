#pragma once

#include "utils/keycodec.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>

// Dialog::exec runs a nested event loop. Each scope routes only the current
// dialog and restores its parent when that loop returns.
class MenuModalInput final {
  public:
    using Handler = std::function<void(KeyCodec::Action)>;

    explicit MenuModalInput(uint64_t& revision):revision(revision) {}

    class Scope final {
      public:
        explicit Scope(MenuModalInput& input, Handler handler = {})
          :input(input), previous(std::move(input.handler)) {
          input.handler = std::move(handler);
          ++input.depth;
          ++input.revision;
          }
        ~Scope() {
          input.handler = std::move(previous);
          --input.depth;
          ++input.revision;
          }
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
      private:
        MenuModalInput& input;
        Handler previous;
      };

    bool active() const { return depth!=0; }
    bool blocked() const { return active() && !handler; }
    void dispatch(KeyCodec::Action action) const {
      // The callback can enter another scope, replacing the stored callable.
      auto current = handler;
      if(current)
        current(action);
      }

  private:
    uint64_t& revision;
    size_t depth = 0;
    Handler handler;
  };

namespace MenuInput {
enum class Item { Command, Value, Horizontal };

inline KeyCodec::Action action(KeyCodec::Action input, Item item) {
  if(input!=KeyCodec::Left && input!=KeyCodec::Right)
    return input;
  if(item==Item::Horizontal)
    return input==KeyCodec::Left ? KeyCodec::Forward : KeyCodec::Back;
  if(item==Item::Value)
    return input;
  return KeyCodec::Idle;
  }

// Shared by the actual save-name dialog's keyboard, mouse and gamepad paths.
// The first terminal decision freezes the result for every input source.
class SaveName final {
  public:
    explicit SaveName(std::string& text):text(text), original(text) {}
    void accept(bool defaultName = false) {
      if(state!=State::Pending)
        return;
      if(defaultName && text.empty())
        text = "Saved game";
      state = State::Accepted;
      }
    void cancel() {
      if(state!=State::Pending)
        return;
      state = State::Cancelled;
      text = original;
      }
    bool isAccepted() const { return state==State::Accepted; }
    bool isPending() const { return state==State::Pending; }
  private:
    // A queued edge can arrive after close(), before Dialog::exec returns.
    // The first terminal decision must remain final for every input source.
    enum class State : uint8_t { Pending, Accepted, Cancelled };
    std::string& text;
    std::string original;
    State state = State::Pending;
  };
}
