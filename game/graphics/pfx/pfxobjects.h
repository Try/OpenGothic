#pragma once

#include <Tempest/Matrix4x4>
#include <Tempest/UniformBuffer>

#include <memory>
#include <list>

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

    void       prepareUniforms();
    void       preFrameUpdate(uint8_t fId);

    void       drawGBuffer    (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);
    void       drawShadow     (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, int layer);
    void       drawTranslucent(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);

  private:
    struct SpriteEmitter {
      zenkit::SpriteAlignment     visualCamAlign = zenkit::SpriteAlignment::none;
      int32_t                     zBias          = 0;
      Tempest::Vec2               decalDim = {};
      std::unique_ptr<ParticleFx> pfx;
      };

    PfxBucket&                    getBucket(const ParticleFx& decl);
    PfxBucket&                    getBucket(const Material& mat, const zenkit::VirtualObject& vob);

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
