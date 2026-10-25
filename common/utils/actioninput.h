#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

// Physical holds and pending gameplay commands have different lifetimes: a tick
// can consume a command while its key remains held, and weapon requests can
// remain pending after release. Keep their ownership independent.
template<size_t Actions, size_t UnboundAction = Actions>
class ActionInput final {
  public:
    enum class Source : uint8_t { Keyboard, Mouse, Touch, Gamepad };
    static constexpr size_t NumOwners = 8;
    using Mask = uint8_t;

    static constexpr size_t ownerIndex(Source source, bool secondary) {
      return size_t(source)*2 + size_t(secondary);
      }
    static constexpr Mask owner(Source source, bool secondary) {
      return Mask(1u << ownerIndex(source, secondary));
      }
    static constexpr Mask sourceMask(Source source) {
      return Mask(owner(source, false) | owner(source, true));
      }

    class Command final {
      public:
        operator bool() const { return active; }
        Command& operator=(bool value) {
          active = value;
          if(!value) {
            origins.fill(0);
            required = {};
            }
          return *this;
          }
        void issue(size_t action, Mask mask) {
          if(!active) {
            origins.fill(0);
            required = {};
            }
          active = true;
          for(size_t o=0; o<NumOwners; ++o)
            if((mask & Mask(1u<<o))!=0 && !hasOrigin(Mask(1u<<o)))
              required[o].fill(0);
          origins[action] |= mask;
          }
        void merge(const Command& other) {
          if(!other.active)
            return;
          if(!active) {
            origins.fill(0);
            required = {};
            }
          active = true;
          for(size_t a=0; a<Actions; ++a) {
            origins[a] |= other.origins[a];
            for(size_t o=0; o<NumOwners; ++o)
              required[o][a] |= other.required[o][a];
            }
          }
        // Dependencies belong to the contributing trigger owner, so an
        // independent direct attack need not keep another owner's modifier.
        void require(size_t action, Mask mask, Mask triggers = Mask(0xff)) {
          for(size_t o=0; o<NumOwners; ++o)
            if(hasOrigin(Mask(1u<<o) & triggers))
              required[o][action] |= mask;
          }
        void share(size_t action, Mask mask, Mask equivalent = Mask(0xff)) {
          if(!active)
            return;
          const auto old = origins[action] & equivalent;
          if(old!=0) {
            for(size_t o=0; o<NumOwners; ++o) {
              if((mask & Mask(1u<<o))==0)
                continue;
              if(!hasOrigin(Mask(1u<<o)))
                required[o].fill(0);
              for(size_t from=0; from<NumOwners; ++from)
                if((old & Mask(1u<<from))!=0)
                  for(size_t a=0; a<Actions; ++a)
                    required[o][a] |= required[from][a];
              }
            origins[action] |= mask;
            }
          // Trigger semantics can differ, but the raw modifier remains shared.
          for(auto& requirements : required)
            if(requirements[action]!=0)
              requirements[action] |= mask;
          }
        // A physical release ends one action, not every command from its device.
        void release(size_t action, Mask mask) {
          origins[action] &= Mask(~mask);
          for(size_t o=0; o<NumOwners; ++o) {
            const bool needed = required[o][action]!=0;
            required[o][action] &= Mask(~mask);
            if(needed && required[o][action]==0)
              removeOrigin(Mask(1u<<o));
            }
          for(size_t o=0; o<NumOwners; ++o)
            if(!hasOrigin(Mask(1u<<o)))
              required[o].fill(0);
          active = active && hasOrigins();
          }
        bool hasOrigin(Mask mask) const {
          if(!active)
            return false;
          for(auto origin : origins)
            if((origin & mask)!=0)
              return true;
          return false;
          }
        void cancel(Mask mask) {
          removeOrigin(mask);
          for(size_t o=0; o<NumOwners; ++o) {
            for(auto& requirement : required[o]) {
              const bool needed = requirement!=0;
              requirement &= Mask(~mask);
              if(needed && requirement==0) {
                removeOrigin(Mask(1u<<o));
                break;
                }
              }
            }
          active = active && hasOrigins();
          }
      private:
        bool hasOrigins() const {
          for(auto origin : origins)
            if(origin!=0)
              return true;
          return false;
          }
        void removeOrigin(Mask mask) {
          for(auto& origin : origins)
            origin &= Mask(~mask);
          for(size_t o=0; o<NumOwners; ++o)
            if((mask & Mask(1u<<o))!=0)
              required[o].fill(0);
          }

        bool active = false;
        std::array<Mask, Actions> origins = {};
        std::array<std::array<Mask, Actions>, NumOwners> required = {};
      };

    bool press(size_t action, Mask mask) {
      // Unbound keys share no physical action identity. Treat every press as
      // independent so a raw-key command cannot be suppressed by another key.
      if(action==UnboundAction)
        return true;
      const bool first = holds[action]==0;
      holds[action] |= mask;
      return first;
      }
    bool release(size_t action, Mask mask) {
      if((holds[action] & mask)==0)
        return false;
      holds[action] &= Mask(~mask);
      return holds[action]==0;
      }
    Mask owners(size_t action) const { return holds[action]; }
    bool held(size_t action) const { return holds[action]!=0; }
    bool held(size_t action, Mask mask) const { return (holds[action]&mask)!=0; }
    void cancel(Mask mask) {
      for(auto& held : holds)
        held &= Mask(~mask);
      }
    void clear() { holds.fill(0); }

  private:
    std::array<Mask, Actions> holds = {};
  };
