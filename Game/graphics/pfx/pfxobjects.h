#pragma once

#include <Tempest/Matrix4x4>
#include <Tempest/UniformBuffer>

#include <memory>
#include <list>
#include <random>

#include "world/objects/pfxemitter.h"
#include "graphics/visualobjects.h"
#include "resources.h"

class SceneGlobals;
class RendererStorage;
class ParticleFx;
class PfxBucket;
class Painter3d;

class PfxObjects final {
  public:
    PfxObjects(const SceneGlobals& scene, VisualObjects& visual);
    ~PfxObjects();

    struct VboContext {
      Tempest::Vec3 left = {};
      Tempest::Vec3 top  = {};
      Tempest::Vec3 z    = {};

      Tempest::Vec3 leftA = {};
      Tempest::Vec3 topA  = {0,1,0};
      };

    PfxEmitter get(const ParticleFx& decl);
    PfxEmitter get(const ZenLoad::zCVobData& vob);

    void       setViewerPos(const Tempest::Vec3& pos);

    void       resetTicks();
    void       tick(uint64_t ticks);

    void       preFrameUpdate(uint8_t fId);

  private:
    struct SpriteEmitter {
      uint8_t                     visualCamAlign = 0;
      int32_t                     zBias          = 0;
      ZMath::float2               decalDim = {};
      std::unique_ptr<ParticleFx> pfx;
      };

    PfxBucket&                    getBucket(const ParticleFx& decl);
    PfxBucket&                    getBucket(const Material& mat, const ZenLoad::zCVobData& vob);

    const SceneGlobals&           scene;
    VisualObjects&                visual;
    std::recursive_mutex          sync;

    std::vector<std::unique_ptr<PfxBucket>> bucket;
    std::vector<SpriteEmitter>              spriteEmit;

    Tempest::Vec3                 viewerPos={};
    uint64_t                      lastUpdate=0;

  friend class PfxEmitter;
  };
