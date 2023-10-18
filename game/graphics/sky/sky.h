#pragma once

#include <Tempest/Matrix4x4>
#include <Tempest/Texture2d>
#include <array>

#include "graphics/sceneglobals.h"
#include "resources.h"

class World;

class Sky final {
  public:
    struct State final {
      const Tempest::Texture2d* lay[2] = {};
      };

    Sky(const SceneGlobals& scene, const World& world, const std::pair<Tempest::Vec3, Tempest::Vec3>& bbox);
    ~Sky();

    void prepareUniforms();
    void setWorld   (const World& world, const std::pair<Tempest::Vec3, Tempest::Vec3>& bbox);
    void updateLight(const int64_t now);

    void prepareSky (Tempest::Encoder<Tempest::CommandBuffer>& p, uint32_t frameId);
    void drawSky    (Tempest::Encoder<Tempest::CommandBuffer>& p, uint32_t frameId);
    void drawSunMoon(Tempest::Encoder<Tempest::CommandBuffer>& p, uint32_t frameId);

    void prepareFog (Tempest::Encoder<Tempest::CommandBuffer>& p, uint32_t frameId);
    void drawFog    (Tempest::Encoder<Tempest::CommandBuffer>& p, uint32_t frameId);

    const Tempest::Texture2d& skyLut()           const;
    const Tempest::Texture2d& clearSkyLut()      const;
    const LightSource&        sunLight()         const { return sun; }
    const Tempest::Vec3&      ambientLight()     const { return ambient; }
    float                     sunIntensity()     const { return GSunIntensity; }
    float                     autoExposure()     const { return exposure; }

    const State&              cloudsDay()   const { return clouds[0]; }
    const State&              cloudsNight() const { return clouds[1]; }
    Tempest::Vec2             cloudsOffset(int layer) const;
    float                     isNight() const;

  private:
    enum Quality : uint8_t {
      None,
      VolumetricLQ,
      VolumetricMQ,
      VolumetricHQ,
      };

    struct UboSky {
      Tempest::Matrix4x4 viewProjectInv;
      Tempest::Vec2      dxy0     = {};
      Tempest::Vec2      dxy1     = {};
      Tempest::Vec3      sunDir   = {};
      float              night    = 1.0;
      Tempest::Vec3      clipInfo;
      float              plPosY   = 0.0;
      float              rayleighScatteringScale = 0;
      float              GSunIntensity = 0;
      float              exposure = 0;
      };

    UboSky                        mkPush(bool lwc=false);
    const Tempest::Texture2d*     skyTexture(std::string_view name, bool day, size_t id);
    const Tempest::Texture2d*     implSkyTexture(std::string_view name, bool day, size_t id);

    void                          setupSettings();
    void                          drawSunMoon(Tempest::Encoder<Tempest::CommandBuffer>& p, uint32_t frameId, bool sun);

    Quality                       quality = Quality::None;

    LightSource                   sun;
    Tempest::Vec3                 ambient;
    float                         exposure = 1;

    Tempest::TextureFormat        lutRGBFormat  = Tempest::TextureFormat::R11G11B10UF;
    Tempest::TextureFormat        lutRGBAFormat = Tempest::TextureFormat::RGBA16F;
    Tempest::Attachment           transLut, multiScatLut, viewLut, viewCldLut;
    Tempest::StorageImage         cloudsLut, fogLut3D, shadowDw;
    Tempest::StorageImage         occlusionLut;

    Tempest::DescriptorSet        uboClouds;
    Tempest::DescriptorSet        uboTransmittance, uboMultiScatLut;
    Tempest::DescriptorSet        uboSkyViewLut, uboSkyViewCldLut;
    Tempest::DescriptorSet        uboFogViewLut3d;
    Tempest::DescriptorSet        uboSky, uboFog, uboFog3d;
    Tempest::DescriptorSet        uboShadowDw, uboOcclusion;

    bool                          lutIsInitialized = false;

    Tempest::DescriptorSet        uboSun, uboMoon;

    const SceneGlobals&           scene;
    State                         clouds[2]; //day, night;

    const Tempest::Texture2d*     sunImg   = &Resources::fallbackBlack();
    float                         sunSize  = 200;
    const Tempest::Texture2d*     moonImg  = &Resources::fallbackBlack();
    float                         moonSize = 400;

    float                         minZ = 0;
    float                         GSunIntensity  = 5.f;
    float                         GMoonIntensity = 0.000025f;
    uint32_t                      occlusionScale = 1;
  };
