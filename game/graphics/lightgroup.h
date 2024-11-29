#pragma once

#include <Tempest/CommandBuffer>
#include <unordered_set>
#include <zenkit/vobs/Light.hh>

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

    Light  add(const zenkit::LightPreset& vob);
    Light  add(const zenkit::VLight& vob);
    Light  add(std::string_view preset);
    size_t size() const { return lightSourceData.size(); }

    void   tick(uint64_t time);
    bool   updateLights();
    auto&  lightsSsbo() const { return lightSourceSsbo; }

    void   preFrameUpdate(uint8_t fId);
    void   prepareGlobals(Tempest::Encoder<Tempest::CommandBuffer> &cmd, uint8_t fId);

    void   dbgLights(DbgPainter& p) const;

  private:
    using Vertex = Resources::VertexL;

    struct Path {
      uint32_t dst;
      uint32_t src;
      uint32_t size;
      };

    struct LightSsbo {
      Tempest::Vec3 pos;
      float         range  = 0;
      Tempest::Vec3 color;
      float         pading = 0;
      };

    struct VsmSsbo {
      uint32_t mask[6];
      };

    size_t                     alloc(bool dynamic);
    void                       free(size_t id);

    void                       markAsDurty(size_t id);
    void                       markAsDurtyNoSync(size_t id);
    void                       resetDurty();

    const zenkit::LightPreset& findPreset(std::string_view preset) const;

    const SceneGlobals&              scene;
    std::vector<zenkit::LightPreset> presets;

    std::mutex                       sync;
    std::vector<size_t>              freeList;
    std::vector<LightSource>         lightSourceDesc;
    std::vector<LightSsbo>           lightSourceData;
    std::unordered_set<size_t>       animatedLights;
    std::vector<uint32_t>            duryBit;

    Tempest::StorageBuffer           lightSourceSsbo;

    Tempest::StorageBuffer           patchSsbo[Resources::MaxFramesInFlight];
    Tempest::DescriptorSet           descPatch[Resources::MaxFramesInFlight];
  };

