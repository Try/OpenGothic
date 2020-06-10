#pragma once

#include <Tempest/Matrix4x4>
#include <Tempest/Texture2d>
#include <array>

#include "graphics/meshobjects.h"
#include "graphics/ubochain.h"
#include "resources.h"

class RendererStorage;
class World;

class Sky final {
  public:
    using Vertex=Resources::VertexFsq;

    Sky(const SceneGlobals& scene);

    void setWorld   (const World& world);
    void draw       (Painter3d& p, uint32_t frameId);
    void setDayNight(float dayF);

  private:
    struct Layer final {
      const Tempest::Texture2d* texture=nullptr;
      };

    struct State final {
      Layer lay[2];
      };

    struct UboGlobal {
      Tempest::Matrix4x4 mvp;
      float              color[4]={};
      float              dxy0[2]={};
      float              dxy1[2]={};
      float              sky [3]={};
      float              night  =1.0;
      };

    struct PerFrame {
      Tempest::UniformBuffer<UboGlobal> uboGpu;
      Tempest::Uniforms                 ubo;
      };

    void                          calcUboParams();
    static std::array<float,3>    mkColor(uint8_t r,uint8_t g,uint8_t b);
    const Tempest::Texture2d*     skyTexture(const char* name, bool day, size_t id);
    const Tempest::Texture2d*     implSkyTexture(const char* name, bool day, size_t id);

    const SceneGlobals&           scene;
    UboGlobal                     uboCpu;
    PerFrame                      perFrame[Resources::MaxFramesInFlight];
    Tempest::VertexBuffer<Vertex> vbo;

    State                         day, night;

    static std::array<float,3>    color;
  };
