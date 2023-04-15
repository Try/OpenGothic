#pragma once

#include <Tempest/Matrix4x4>
#include <Tempest/Texture2d>
#include <array>

#include "graphics/sceneglobals.h"
#include "resources.h"

class World;

class Sky final {
  public:
    using Vertex=Resources::VertexFsq;

    Sky(const SceneGlobals& scene, const World& world, const std::pair<Tempest::Vec3, Tempest::Vec3>& bbox);
    ~Sky();

    void setupUbo();
    void setWorld   (const World& world, const std::pair<Tempest::Vec3, Tempest::Vec3>& bbox);
    void updateLight(const int64_t now);

    void prepareSky (Tempest::Encoder<Tempest::CommandBuffer>& p, uint32_t frameId);
    void drawSky    (Tempest::Encoder<Tempest::CommandBuffer>& p, uint32_t frameId);

    void prepareFog (Tempest::Encoder<Tempest::CommandBuffer>& p, uint32_t frameId);
    void drawFog    (Tempest::Encoder<Tempest::CommandBuffer>& p, uint32_t frameId);

    const Tempest::Texture2d& skyLut()           const;
    const LightSource&        sunLight()         const { return sun; }
    const Tempest::Vec3&      ambientLight()     const { return ambient; }
    float                     sunIntensity()     const { return GSunIntensity; }
    float                     autoExposure()     const { return exposureInv; }

  private:
    enum Quality : uint8_t {
      Exponential,
      VolumetricLQ,
      VolumetricHQ,
      };

    struct Layer final {
      const Tempest::Texture2d* texture=nullptr;
      };

    struct State final {
      Layer lay[2];
      };

    struct UboSky {
      Tempest::Matrix4x4 viewProjectInv;
      float              dxy0[2]  = {};
      float              dxy1[2]  = {};
      Tempest::Vec3      sunDir   = {};
      float              night    = 1.0;
      Tempest::Vec3      clipInfo;
      float              plPosY   = 0.0;
      float              rayleighScatteringScale = 0;
      float              GSunIntensity = 0;
      float              exposureInv = 0;
      };

    UboSky                        mkPush();
    const Tempest::Texture2d*     skyTexture(std::string_view name, bool day, size_t id);
    const Tempest::Texture2d*     implSkyTexture(std::string_view name, bool day, size_t id);

    void                          setupSettings();
    void                          drawSunMoon(Tempest::Encoder<Tempest::CommandBuffer>& p, uint32_t frameId, bool sun);

    float                         isNight() const;

    Quality                       quality = Quality::Exponential;

    LightSource                   sun;
    Tempest::Vec3                 ambient;
    float                         exposureInv = 1;

    Tempest::TextureFormat        lutRGBFormat  = Tempest::TextureFormat::R11G11B10UF;
    Tempest::TextureFormat        lutRGBAFormat = Tempest::TextureFormat::RGBA16F;
    Tempest::Attachment           transLut, multiScatLut, viewLut, fogLut;
    Tempest::StorageImage         cloudsLut, fogLut3D, shadowDw;
    Tempest::StorageImage         occlusionLut;

    Tempest::DescriptorSet        uboClouds;
    Tempest::DescriptorSet        uboTransmittance;
    Tempest::DescriptorSet        uboMultiScatLut, uboSkyViewLut;
    Tempest::DescriptorSet        uboFogViewLut,   uboFogViewLut3d[Resources::MaxFramesInFlight];
    Tempest::DescriptorSet        uboSky;
    Tempest::DescriptorSet        uboFog[Resources::MaxFramesInFlight], uboFog3d[Resources::MaxFramesInFlight];
    Tempest::DescriptorSet        uboShadowDw, uboOcclusion[Resources::MaxFramesInFlight];

    bool                          lutIsInitialized = false;

    Tempest::DescriptorSet        uboSun, uboMoon;

    const SceneGlobals&           scene;
    State                         day, night;

    const Tempest::Texture2d*     sunImg   = &Resources::fallbackBlack();
    float                         sunSize  = 200;
    const Tempest::Texture2d*     moonImg  = &Resources::fallbackBlack();
    float                         moonSize = 400;

    float                         minZ = 0;
    float                         GSunIntensity = 20.f;
  };
