#pragma once

#include <Tempest/Matrix4x4>
#include <Tempest/Texture2d>

#include <daedalus/ZString.h>
#include <vector>
#include <memory>
#include <cstdint>

class World;
class VisualFx;

class GlobalEffects {
  public:
    GlobalEffects(World& owner);

    void   tick(uint64_t dt);
    void   startEffect(const Daedalus::ZString& what, float len, const Daedalus::ZString* argv, size_t argc);
    void   stopEffect (const VisualFx& vfx);

    void   scaleTime(uint64_t& dt);
    void   morph(Tempest::Matrix4x4& proj);

  private:
    enum Type : uint8_t {
      SLOW_TIME
      };

    struct Effect {
      virtual ~Effect(){}
      uint64_t timeUntil = 0;
      };

    struct SlowTime:Effect {
      uint64_t mul = 0;
      uint64_t div = 0;
      };

    struct ScreenBlend:Effect {
      ScreenBlend(const char* tex);
      std::vector<Tempest::Texture2d> tex;
      };

    struct Morph:Effect {
      float amplitude = 1;
      float speed     = 1;
      float baseX     = 0;
      float baseY     = 0;
      };

    template<class T>
    void    tick(uint64_t dt, std::vector<T>& eff);

    Effect& create        (const Daedalus::ZString& what, const Daedalus::ZString* argv, size_t argc);
    Effect& addSlowTime   (const Daedalus::ZString* argv, size_t argc);
    Effect& addScreenBlend(const Daedalus::ZString* argv, size_t argc);
    Effect& addMorphFov   (const Daedalus::ZString* argv, size_t argc);

    World&   owner;
    uint64_t timeWrldRem = 0;

    std::vector<SlowTime>    timeEff;
    std::vector<ScreenBlend> scrEff;
    std::vector<Morph>       morphEff;
  };

