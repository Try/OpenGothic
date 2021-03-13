#pragma once

#include <Tempest/CommandBuffer>
#include <memory>

#include "graphics/dynamic/frustrum.h"
#include "bounds.h"
#include "lightsource.h"
#include "resources.h"

class DbgPainter;
class SceneGlobals;
class World;

class LightGroup final {
  public:
    LightGroup(const SceneGlobals& scene);

    class Light final {
      public:
        Light() = default;
        Light(LightGroup& owner, const ZenLoad::zCVobData& vob);
        Light(LightGroup& owner);
        Light(World& owner, const ZenLoad::zCVobData& vob);
        Light(World& owner);

        Light(Light&& other);
        Light& operator = (Light&& other);
        ~Light();

        void setPosition(float x, float y, float z);
        void setPosition(const Tempest::Vec3& p);

        void setRange(float r);
        void setColor(const Tempest::Vec3& c);

      private:
        Light(LightGroup& l, size_t id):light(&l), id(id) {}
        LightGroup* light = nullptr;
        size_t      id    = 0;

      friend class LightGroup;
      };

    void   dbgLights(DbgPainter& p) const;

    size_t get(const Bounds& area, const LightSource** out, size_t maxOut) const;

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
      const LightSource**  b     = nullptr;
      size_t               count = 0;
      };

    void        free(size_t id);
    size_t      implGet(const Bvh& index, const Bounds& area, const LightSource** out, size_t maxOut) const;
    void        mkIndex() const;
    void        mkIndex(Bvh& id, const LightSource** b, size_t count, int depth) const;
    void        clearIndex();
    static bool isIntersected(const Bounds& a,const Bounds& b);
    void        buildVbo(uint8_t fId);
    void        buildVbo(Vertex* out, const LightSource& l);

    const SceneGlobals&               scene;

    struct Chunk {
      Tempest::VertexBufferDyn<Vertex> vboGpu [Resources::MaxFramesInFlight];
      bool                             updated[Resources::MaxFramesInFlight] = {};
      };
    std::vector<Chunk>                chunks;

    std::vector<Vertex>               vboCpu;
    Tempest::IndexBuffer<uint16_t>    iboGpu;

    Tempest::Uniforms                 ubo[Resources::MaxFramesInFlight];
    Tempest::UniformBuffer<Ubo>       uboBuf[Resources::MaxFramesInFlight];

    std::recursive_mutex              sync;
    std::vector<LightSource>          light;
    std::vector<size_t>               dynamicState;
    std::vector<size_t>               freeList;
    mutable std::vector<const LightSource*> indexPtr;
    mutable Bvh                       index;
    mutable bool                      fullGpuUpdate = false;
  };

