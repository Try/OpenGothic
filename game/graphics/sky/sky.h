#pragma once

#include <Tempest/Matrix4x4>
#include <Tempest/Texture2d>
#include <array>

#include "graphics/meshobjects.h"
#include "resources.h"

class RendererStorage;
class World;

class Sky final {
  public:
    using Vertex=Resources::VertexFsq;

    Sky(const SceneGlobals& scene);

    void setupUbo();
    void setWorld   (const World& world);
    void setDayNight(float dayF);

    void drawSky    (Tempest::Encoder<Tempest::CommandBuffer>& p, uint32_t frameId);
    void drawFog    (Tempest::Encoder<Tempest::CommandBuffer>& p, uint32_t frameId);

  private:
    struct Layer final {
      const Tempest::Texture2d* texture=nullptr;
      };

    struct State final {
      Layer lay[2];
      };

    struct UboSky {
      Tempest::Matrix4x4 mvpInv;
      float              dxy0[2]  = {};
      float              dxy1[2]  = {};
      Tempest::Vec3      sunDir   = {};
      float              night    = 1.0;
      float              plPosY   = 0.0;
      };

    struct UboFog {
      Tempest::Matrix4x4 mvp;
      Tempest::Matrix4x4 mvpInv;
      };

    struct PerFrame {
      Tempest::UniformBuffer<UboSky> uboSkyGpu;
      Tempest::DescriptorSet         uboSky;
      Tempest::DescriptorSet         uboFog;
      };

    static std::array<float,3>    mkColor(uint8_t r,uint8_t g,uint8_t b);
    const Tempest::Texture2d*     skyTexture(const char* name, bool day, size_t id);
    const Tempest::Texture2d*     implSkyTexture(const char* name, bool day, size_t id);

    const SceneGlobals&           scene;
    float                         nightFlt = 0.f;
    PerFrame                      perFrame[Resources::MaxFramesInFlight];
    Tempest::VertexBuffer<Vertex> vbo;

    State                         day, night;
    const Tempest::Texture2d*     sun = &Resources::fallbackBlack();
  };
