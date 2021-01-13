#pragma once

#include <Tempest/Matrix4x4>
#include <Tempest/Painter>
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
    void   scrBlend(Tempest::Painter& p, const Tempest::Rect& rect);

  private:
    struct Effect {
      virtual ~Effect(){}
      uint64_t timeUntil = 0;
      uint64_t timeStart = 0;
      };

    struct SlowTime:Effect {
      // world  time scaler
      uint64_t mul = 0;
      uint64_t div = 0;
      // TODO: player time scaler
      };

    struct ScreenBlend:Effect {
      float          loop  = 0;                      // screenblend loop duration
      Tempest::Color cl    = {1,1,1,1};              // screenblend color
      float          inout = 0;                      // screenblend in/out duration
      std::vector<const Tempest::Texture2d*> frames; // screenblend texture
      size_t         fps   = 1;                      // tex ani fps
      };

    struct Morph:Effect {
      float amplitude = 1; // fov morph amplitude scaler
      float speed     = 1; // fov morph speed scaler
      float baseX     = 0; // fov base x (leave empty for default)
      float baseY     = 0; // fov base y (leave empty for default)
      };

    struct Quake:Effect {
      };

    template<class T>
    void    tick(uint64_t dt, std::vector<T>& eff);

    Effect& create        (const Daedalus::ZString& what, const Daedalus::ZString* argv, size_t argc);
    Effect& addSlowTime   (const Daedalus::ZString* argv, size_t argc);
    Effect& addScreenBlend(const Daedalus::ZString* argv, size_t argc);
    Effect& addMorphFov   (const Daedalus::ZString* argv, size_t argc);
    Effect& addEarthQuake (const Daedalus::ZString* argv, size_t argc);


    static Tempest::Color parseColor(const char* c);

    World&                   owner;
    uint64_t                 timeWrldRem = 0;

    std::vector<SlowTime>    timeEff;
    std::vector<ScreenBlend> scrEff;
    std::vector<Morph>       morphEff;
    std::vector<Quake>       quakeEff;
  };

