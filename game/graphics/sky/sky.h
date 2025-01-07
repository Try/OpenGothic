#pragma once

#include <Tempest/Matrix4x4>
#include <Tempest/Texture2d>

#include "graphics/lightsource.h"
#include "graphics/sceneglobals.h"
#include "resources.h"

class World;

class Sky final {
  public:
    struct State final {
      const Tempest::Texture2d* lay[2] = {};
      };

    Sky(const SceneGlobals& scene, const World& world);
    ~Sky();

    void updateLight(const int64_t now);

    const LightSource&        sunLight()         const { return sun;     }
    const Tempest::Vec3&      ambientLight()     const { return ambient; }
    float                     sunIntensity()     const { return GSunIntensity;  }
    float                     moonIntensity()    const { return GMoonIntensity; }

    const Tempest::Texture2d& sunImage()  const { return *sunImg;  }
    const Tempest::Texture2d& moonImage() const { return *moonImg; }

    const State&              cloudsDay()   const { return clouds[0]; }
    const State&              cloudsNight() const { return clouds[1]; }
    Tempest::Vec2             cloudsOffset(int layer) const;
    float                     isNight() const;

  private:
    enum Quality : uint8_t {
      None,
      VolumetricLQ,
      VolumetricHQ,
      VolumetricHQVsm,
      PathTrace,
      };

    struct UboSky {
      Tempest::Matrix4x4 viewProjectInv;
      float              plPosY = 0.0;
      float              rayleighScatteringScale = 0;
      };

    const Tempest::Texture2d*     skyTexture(std::string_view name, bool day, size_t id);
    const Tempest::Texture2d*     implSkyTexture(std::string_view name, bool day, size_t id);

    LightSource                   sun;
    Tempest::Vec3                 ambient;

    const SceneGlobals&           scene;
    State                         clouds[2]; //day, night;

    const Tempest::Texture2d*     sunImg   = &Resources::fallbackBlack();
    const Tempest::Texture2d*     moonImg  = &Resources::fallbackBlack();

    float                         GSunIntensity  = 0;
    float                         GMoonIntensity = 0;
  };
