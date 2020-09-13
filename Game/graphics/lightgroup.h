#pragma once

#include <Tempest/CommandBuffer>
#include <memory>

#include "bounds.h"
#include "light.h"
#include "resources.h"

class SceneGlobals;

class LightGroup final {
  public:
    LightGroup(const SceneGlobals& scene);

    size_t size() const;
    const Light& operator [](size_t i) const { return light[i]; }

    void   set(const std::vector<Light>& light);
    size_t get(const Bounds& area, const Light** out, size_t maxOut) const;
    void   tick(uint64_t time);
    void   preFrameUpdate(uint8_t fId);
    void   draw(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);
    void   setupUbo();

  private:
    using Vertex = Resources::VertexL;

    struct Ubo {
      Tempest::Matrix4x4 mvp;
      Tempest::Matrix4x4 mvpInv;
      };

    struct Bvh {
      std::unique_ptr<Bvh> next[2];
      Bounds               bbox;
      const Light*         b = nullptr;
      size_t               count=0;
      };

    size_t      implGet(const Bvh& index, const Bounds& area, const Light** out, size_t maxOut) const;
    void        mkIndex(Bvh& id, Light* b, size_t count, int depth);
    static bool isIntersected(const Bounds& a,const Bounds& b);
    void        buildVbo();

    const SceneGlobals& scene;

    std::vector<Vertex>              vboCpu;
    Tempest::VertexBufferDyn<Vertex> vboGpu[Resources::MaxFramesInFlight];

    Tempest::Uniforms                ubo[Resources::MaxFramesInFlight];

    std::vector<Light>  light;
    std::vector<Light*> dynamicState;
    Bvh                 index;
  };

