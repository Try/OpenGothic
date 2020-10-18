#pragma once

#include <Tempest/CommandBuffer>
#include <memory>

#include "graphics/dynamic/frustrum.h"
#include "bounds.h"
#include "light.h"
#include "resources.h"

class SceneGlobals;

class LightGroup final {
  public:
    LightGroup(const SceneGlobals& scene);

    void   dbgLights(Tempest::Painter& p, const Tempest::Matrix4x4& vp, uint32_t vpWidth, uint32_t vpHeight) const;
    size_t size() const;
    const Light& operator [](size_t i) const { return light[i]; }

    size_t add(Light&& l);

    size_t get(const Bounds& area, const Light** out, size_t maxOut) const;
    void   tick(uint64_t time);
    void   preFrameUpdate(uint8_t fId);
    void   draw(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);
    void   setupUbo();

  private:
    using Vertex = Resources::VertexL;

    enum {
      CHUNK_SIZE=256
      };

    struct Ubo {
      Tempest::Matrix4x4 mvp;
      Tempest::Matrix4x4 mvpInv;
      Frustrum           fr;
      };

    struct Bvh {
      std::unique_ptr<Bvh> next[2];
      Bounds               bbox;
      const Light**        b = nullptr;
      size_t               count=0;
      };

    size_t      implGet(const Bvh& index, const Bounds& area, const Light** out, size_t maxOut) const;
    void        mkIndex() const;
    void        mkIndex(Bvh& id, const Light** b, size_t count, int depth) const;
    void        clearIndex();
    static bool isIntersected(const Bounds& a,const Bounds& b);
    void        buildVbo(uint8_t fId);

    const SceneGlobals& scene;

    struct Chunk {
      Tempest::VertexBufferDyn<Vertex> vboGpu[Resources::MaxFramesInFlight];
      };
    std::vector<Chunk>               chunks;

    std::vector<Vertex>              vboCpu;
    Tempest::IndexBuffer<uint16_t>   iboGpu;

    Tempest::Uniforms                ubo[Resources::MaxFramesInFlight];
    Tempest::UniformBuffer<Ubo>      uboBuf[Resources::MaxFramesInFlight];

    std::vector<Light>                light;
    std::vector<size_t>               dynamicState;
    mutable std::vector<const Light*> indexPtr;
    mutable Bvh                       index;
  };

