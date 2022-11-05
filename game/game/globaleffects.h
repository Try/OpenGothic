#pragma once

#include <Tempest/Matrix4x4>
#include <Tempest/Painter>
#include <Tempest/Texture2d>

#include <vector>
#include <memory>
#include <cstdint>

class World;
class VisualFx;
class GlobalFx;

class GlobalEffects {
  public:
    GlobalEffects(World& owner);

    void     tick(uint64_t dt);
    GlobalFx startEffect(std::string_view what, uint64_t len, const std::string* argv, size_t argc);
    void     stopEffect (const VisualFx& vfx);

    void     scaleTime(uint64_t& dt);
    void     morph(Tempest::Matrix4x4& proj);
    void     scrBlend(Tempest::Painter& p, const Tempest::Rect& rect);

  private:
    struct Effect {
      virtual ~Effect(){}
      virtual void stop();
      uint64_t timeUntil = 0;
      uint64_t timeStart = 0;
      uint64_t timeLen   = 0;
      uint64_t timeLoop  = 0;
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

    GlobalFx create        (std::string_view what, const std::string* argv, size_t argc);
    GlobalFx addSlowTime   (const std::string* argv, size_t argc);
    GlobalFx addScreenBlend(const std::string* argv, size_t argc);
    GlobalFx addMorphFov   (const std::string* argv, size_t argc);
    GlobalFx addEarthQuake (const std::string* argv, size_t argc);


    static Tempest::Color parseColor(std::string_view c);

    World&   owner;
    uint64_t timeWrldRem = 0;

    std::vector<std::shared_ptr<SlowTime>>    timeEff;
    std::vector<std::shared_ptr<ScreenBlend>> scrEff;
    std::vector<std::shared_ptr<Morph>>       morphEff;
    std::vector<std::shared_ptr<Quake>>       quakeEff;
  friend class GlobalFx;
  };

