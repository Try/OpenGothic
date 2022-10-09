#pragma once

#include <Tempest/CommandBuffer>
#include <phoenix/vobs/light.hh>
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
        Light(LightGroup& owner, const phoenix::vobs::light& vob);
        Light(LightGroup& owner, const phoenix::vobs::light_preset& vob);
        Light(LightGroup& owner);
        Light(World& owner, const phoenix::vobs::light_preset& vob);
        Light(World& owner, const phoenix::vobs::light& vob);
        Light(World& owner, std::string_view preset);
        Light(World& owner);

        Light(Light&& other);
        Light& operator = (Light&& other);
        ~Light();

        void     setPosition(float x, float y, float z);
        void     setPosition(const Tempest::Vec3& p);

        void     setRange (float r);
        void     setColor (const Tempest::Vec3& c);
        void     setColor (const std::vector<Tempest::Vec3>& c, float fps, bool smooth);
        void     setTimeOffset(uint64_t t);

        uint64_t effectPrefferedTime() const;

      private:
        Light(LightGroup& l, size_t id):owner(&l), id(id) {}
        LightGroup* owner = nullptr;
        size_t      id    = 0;

      friend class LightGroup;
      };

    void   dbgLights(DbgPainter& p) const;

    void   tick(uint64_t time);
    void   preFrameUpdate(uint8_t fId);
    void   draw(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);
    void   setupUbo();

  private:
    using Vertex = Resources::VertexL;

    enum {
      CHUNK_SIZE=256
      };

    const size_t staticMask = (size_t(1) << (sizeof(size_t)*8-1));

    struct Ubo {
      Tempest::Matrix4x4 mvp;
      Tempest::Matrix4x4 mvpInv;
      float              fr[6][4] = {};
      };

    struct LightSsbo {
      Tempest::Vec3 pos;
      float         range  = 0;
      Tempest::Vec3 color;
      float         pading = 0;
      };

    struct LightBucket {
      std::vector<LightSource> light;
      std::vector<LightSsbo>   data;
      Tempest::StorageBuffer   ssbo[Resources::MaxFramesInFlight];
      bool                     updated[Resources::MaxFramesInFlight] = {};

      std::vector<size_t>      freeList;
      Tempest::DescriptorSet   ubo[Resources::MaxFramesInFlight];

      size_t                   alloc();
      void                     free(size_t id);
      };

    size_t                             alloc(bool dynamic);
    void                               free(size_t id);

    LightSsbo&                         get (size_t id);
    LightSource&                       getL(size_t id);

    Tempest::RenderPipeline&           shader() const;

    const phoenix::vobs::light_preset& findPreset(std::string_view preset) const;

    const SceneGlobals&                      scene;
    std::vector<phoenix::vobs::light_preset> presets;

    Tempest::UniformBuffer<Ubo>          uboBuf[Resources::MaxFramesInFlight];

    Tempest::IndexBuffer<uint16_t>       ibo;
    Tempest::VertexBuffer<Tempest::Vec3> vbo;

    std::recursive_mutex                 sync;
    LightBucket                          bucketSt, bucketDyn;
  };

