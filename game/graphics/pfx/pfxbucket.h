#pragma once

#include <Tempest/VertexBuffer>
#include <vector>

#include "graphics/pfx/pfxobjects.h"
#include "graphics/objectsbucket.h"
#include "resources.h"

class ParticleFx;
class VisualObjects;

class PfxBucket {
  public:
    using Vertex = Resources::Vertex;

    PfxBucket(const ParticleFx &decl, PfxObjects& parent, const SceneGlobals& scene, VisualObjects& visual);
    ~PfxBucket();

    enum AllocState: uint8_t {
      S_Free,
      S_Fade,
      S_Inactive,
      S_Active,
      };

    struct ImplEmitter final {
      AllocState    st           = S_Free;
      size_t        block        = size_t(-1);

      Tempest::Vec3 pos          = {};
      Tempest::Vec3 direction[3] = {{1,0,0}, {0,1,0}, {0,0,1}};
      bool          isLoop       = false;

      const Npc*    targetNpc    = nullptr;

      const PfxEmitterMesh* mesh = nullptr;
      const Pose*           pose = nullptr;

      uint64_t      waitforNext = 0;
      std::unique_ptr<PfxEmitter> next;
      };

    struct PfxState {
      Tempest::Vec3 pos;
      uint32_t      color  = 0;
      Tempest::Vec3 size;
      uint32_t      bits0  = 0;
      Tempest::Vec3 dir;
      uint32_t      colorB = 0;
      };

    const ParticleFx&           decl;
    PfxObjects&                 parent;

    bool                        isEmpty() const;

    void                        prepareUniforms(const SceneGlobals& scene);

    void                        preFrameUpdate(uint8_t fId);
    void                        drawGBuffer    (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);
    void                        drawShadow     (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, int layer);
    void                        drawTranslucent(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);

    size_t                      allocEmitter();
    void                        freeEmitter(size_t& id);

    ImplEmitter&                get(size_t id) { return impl[id]; }
    void                        tick(uint64_t dt, const Tempest::Vec3& viewPos);
    void                        buildSsbo();

  private:
    enum UboLinkpackage : uint8_t {
      L_Scene    = 0,
      L_Matrix   = 1,
      L_MeshDesc = L_Matrix,
      L_Bucket   = 2,
      L_Ibo      = 3,
      L_Vbo      = 4,
      L_Diffuse  = 5,
      L_Shadow0  = 6,
      L_Shadow1  = 7,
      L_Pfx      = 8,
      L_SceneClr = 10,
      L_GDepth   = 11,
      L_HiZ      = 12,
      };

    struct Block final {
      bool          allocated = false;
      uint64_t      timeTotal = 0;

      size_t        offset    = 0;
      size_t        count     = 0;

      Tempest::Vec3 pos       = {};
      };

    struct Trail final {
      Tempest::Vec3 pos;
      uint64_t      time = 0;
      };

    struct ParState final {
      uint16_t      life=0, maxLife=1;
      Tempest::Vec3 pos, dir;

      std::vector<Trail> trail;

      float         lifeTime() const;
      };

    struct Draw {
      const Tempest::RenderPipeline* pMain   = nullptr;
      const Tempest::RenderPipeline* pShadow = nullptr;

      Tempest::DescriptorSet         ubo[SceneGlobals::V_Count];
      Tempest::StorageBuffer         pfxGpu;

      bool                      isEmpty() const;
      void                      prepareUniforms(const SceneGlobals& scene, const Material& mat);
      void                      preframeUpdate(const SceneGlobals& scene, const Material& mat);
      void                      setPfxData(const Tempest::StorageBuffer& ssbo);
      };

    void                        drawCommon(Tempest::Encoder<Tempest::CommandBuffer>& cmd, SceneGlobals::VisCamera view, const Draw& itm);

    void                        tickEmit(Block& p, ImplEmitter& emitter, uint64_t emited);
    bool                        shrink();

    size_t                      allocBlock();
    void                        freeBlock(size_t& s);

    static float                randf();
    static float                randf(float base, float var);

    Block&                      getBlock(ImplEmitter& emitter);
    Block&                      getBlock(PfxEmitter&  emitter);

    void                        init     (Block& block, ImplEmitter& emitter, size_t particle);
    void                        finalize (size_t particle);
    void                        tick     (Block& sys, ImplEmitter& emitter, size_t particle, uint64_t dt);
    void                        tickTrail(ParState& ps, ImplEmitter& emitter, uint64_t dt);

    void                        implTickCommon(uint64_t dt, const Tempest::Vec3& viewPos);
    void                        implTickDecals(uint64_t dt, const Tempest::Vec3& viewPos);

    void                        buildSsboTrails();
    void                        buildBilboard(PfxState& v, const Block& p, const ParState& ps, const uint32_t color,
                                              float szX, float szY, float szZ);
    void                        buildTrailSegment(PfxState& v, const Trail& a, const Trail& b, float maxT);
    uint32_t                    mkTrailColor(float clA) const;

    VisualObjects&              visual;

    Draw                        item  [Resources::MaxFramesInFlight];
    std::vector<PfxState>       pfxCpu;

    Draw                        itemTrl[Resources::MaxFramesInFlight];
    std::vector<PfxState>       trlCpu;

    uint64_t                    maxTrlTime = 0;
    size_t                      blockSize = 0;

    std::vector<ParState>       particles;
    std::vector<ImplEmitter>    impl;
    std::vector<Block>          block;
    bool                        forceUpdate[Resources::MaxFramesInFlight] = {};

    static std::mt19937         rndEngine;

    friend class PfxEmitter;
  };

