#pragma once

#include <Tempest/Matrix4x4>
#include <Tempest/UniformBuffer>

#include <memory>
#include <list>
#include <random>

#include "world/objects/pfxemitter.h"
#include "graphics/visualobjects.h"

class SceneGlobals;
class ParticleFx;
class PfxBucket;
class WorldView;

class PfxObjects final {
  public:
    PfxObjects(WorldView& world, const SceneGlobals& scene, VisualObjects& visual);
    ~PfxObjects();

    static constexpr const float viewRage = 4000.f;

    void       setViewerPos(const Tempest::Vec3& pos);

    void       resetTicks();
    void       tick(uint64_t ticks);
    bool       isInPfxRange(const Tempest::Vec3& pos) const;

    void       preFrameUpdate(uint8_t fId);

  private:
    struct SpriteEmitter {
      phoenix::sprite_alignment   visualCamAlign = phoenix::sprite_alignment::none;
      int32_t                     zBias          = 0;
      Tempest::Vec2               decalDim = {};
      std::unique_ptr<ParticleFx> pfx;
      };

    PfxBucket&                    getBucket(const ParticleFx& decl);
    PfxBucket&                    getBucket(const Material& mat, const phoenix::vob& vob);

    WorldView&                    world;
    const SceneGlobals&           scene;
    VisualObjects&                visual;
    std::recursive_mutex          sync;

    std::list<PfxBucket>          bucket;
    std::vector<SpriteEmitter>    spriteEmit;

    Tempest::Vec3                 viewerPos={};
    uint64_t                      lastUpdate=0;

  friend class PfxEmitter;
  friend class TrlObjects;
  };
