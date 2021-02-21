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

    PfxBucket(const ParticleFx &ow, PfxObjects* parent, VisualObjects& visual);
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

      Tempest::Vec3 target       = {};
      bool          hasTarget    = false;

      const PfxEmitterMesh* mesh = nullptr;
      const Pose*           pose = nullptr;

      uint64_t      waitforNext = 0;
      std::unique_ptr<PfxEmitter> next;
      };

    ObjectsBucket::Item         item;
    Tempest::VertexBufferDyn<Vertex> vboGpu[Resources::MaxFramesInFlight];
    std::vector<Vertex>         vboCpu;

    const ParticleFx*           owner=nullptr;
    PfxObjects*                 parent=nullptr;
    size_t                      blockSize=0;

    bool                        isEmpty() const;

    size_t                      allocEmitter();
    void                        freeEmitter(size_t& id);

    ImplEmitter&                get(size_t id) { return impl[id]; }
    void                        tick(uint64_t dt, const Tempest::Vec3& viewPos);
    void                        buildVbo(const PfxObjects::VboContext& ctx);

  private:
    struct Block final {
      uint64_t      timeTotal    = 0;

      size_t        offset       = 0;
      size_t        count        = 0;

      Tempest::Vec3 pos          = {};
      Tempest::Vec3 direction[3] = {};
      bool          alive        = true;
      };

    struct ParState final {
      uint16_t      life=0,maxLife=1;
      Tempest::Vec3 pos, dir;

      float         velocity=0;
      float         rotation=0;

      float         lifeTime() const;
      };

    void                        tickEmit(Block& p, ImplEmitter& emitter, uint64_t emited);
    bool                        shrink();

    size_t                      allocBlock();
    void                        freeBlock(size_t& s);

    static float                randf();
    static float                randf(float base, float var);

    Block&                      getBlock(ImplEmitter& emitter);
    Block&                      getBlock(PfxEmitter&  emitter);

    void                        init    (Block& block, ImplEmitter& emitter, size_t particle);
    void                        finalize(size_t particle);
    void                        tick    (Block& sys, ImplEmitter& emitter, size_t particle, uint64_t dt);

    VisualObjects&              visual;
    std::vector<ParState>       particles;
    std::vector<ImplEmitter>    impl;
    std::vector<Block>          block;

    static std::mt19937         rndEngine;

    friend class PfxEmitter;
  };

