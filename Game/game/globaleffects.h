#pragma once

#include <daedalus/ZString.h>
#include <vector>
#include <memory>
#include <cstdint>

class World;

class GlobalEffects {
  public:
    GlobalEffects(World& owner);

    void tick(uint64_t dt);
    void start(const Daedalus::ZString& what, uint64_t len, const Daedalus::ZString* argv, size_t argc);

    void scaleTime(uint64_t& dt);

  private:
    enum Type : uint8_t {
      SLOW_TIME
      };

    struct Effect {
      virtual ~Effect(){}
      virtual void scaleTime(GlobalEffects& owner, uint64_t& dt) = 0;

      uint64_t timeUntil = 0;
      };

    struct SlowTime:Effect {
      SlowTime(uint64_t mul, uint64_t div);
      void   scaleTime(GlobalEffects& owner, uint64_t& dt) override;
      uint64_t mul = 0;
      uint64_t div = 0;
      };

    void     addSlowTime(uint64_t len, const Daedalus::ZString* argv, size_t argc);

    World&   owner;
    uint64_t timeWrldRem = 0;

    std::vector<std::unique_ptr<Effect>> eff;
  };

