#pragma once

#include <Tempest/Matrix4x4>
#include <Tempest/Texture2d>
#include <array>

#include "graphics/meshobjects.h"
#include "resources.h"

class World;

class Sky final {
  public:
    using Vertex=Resources::VertexFsq;

    Sky(const SceneGlobals& scene, const World& world, const std::pair<Tempest::Vec3, Tempest::Vec3>& bbox);
    ~Sky();

    void setupUbo();
    void setWorld   (const World& world, const std::pair<Tempest::Vec3, Tempest::Vec3>& bbox);

    void prepareSky (Tempest::Encoder<Tempest::CommandBuffer>& p, uint32_t frameId);
    void drawSky    (Tempest::Encoder<Tempest::CommandBuffer>& p, uint32_t frameId);
    void drawFog    (Tempest::Encoder<Tempest::CommandBuffer>& p, uint32_t frameId);

    const Tempest::Texture2d& skyLut() const;

  private:
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
      };

    UboSky                        mkPush();
    const Tempest::Texture2d*     skyTexture(std::string_view name, bool day, size_t id);
    const Tempest::Texture2d*     implSkyTexture(std::string_view name, bool day, size_t id);

    void                          setupSettings();
    bool                          zFogRadial = false;

    Tempest::TextureFormat        lutFormat = Tempest::TextureFormat::RGBA32F;
    Tempest::Attachment           transLut, multiScatLut, viewLut, fogLut;
    Tempest::StorageImage         fogLut3D;
    Tempest::DescriptorSet        uboMultiScatLut, uboSkyViewLut;
    Tempest::DescriptorSet        uboFogViewLut,   uboFogViewLut3d[Resources::MaxFramesInFlight];
    Tempest::DescriptorSet        uboSky, uboSky3d, uboFog, uboFog3d;
    bool                          lutIsInitialized = false;

    const SceneGlobals&           scene;
    State                         day, night;
    const Tempest::Texture2d*     sun = &Resources::fallbackBlack();

    float                         minZ = 0;
  };
