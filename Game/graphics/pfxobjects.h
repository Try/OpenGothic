#pragma once

#include <Tempest/Matrix4x4>
#include <Tempest/UniformBuffer>

#include <memory>
#include <list>
#include <random>

#include "visualobjects.h"
#include "resources.h"

class SceneGlobals;
class RendererStorage;
class ParticleFx;
class Painter3d;

class PfxObjects final {
  private:
    struct Bucket;

  public:
    PfxObjects(const SceneGlobals& scene, VisualObjects& visual);
    ~PfxObjects();

    class Emitter final {
      public:
        Emitter()=default;
        ~Emitter();
        Emitter(Emitter&&);
        Emitter& operator=(Emitter&& b);

        Emitter(const Emitter&)=delete;

        bool   isEmpty() const { return bucket==nullptr; }
        void   setPosition (float x,float y,float z);
        void   setTarget   (const Tempest::Vec3& pos);
        void   setDirection(const Tempest::Matrix4x4& pos);
        void   setObjMatrix(const Tempest::Matrix4x4& mt);
        void   setActive(bool act);
        bool   isActive() const;

      private:
        Emitter(Bucket &b,size_t id);

        Bucket*         bucket = nullptr;
        size_t          id     = size_t(-1);

      friend class PfxObjects;
      };

    Emitter get(const ParticleFx& decl);
    Emitter get(const ZenLoad::zCVobData& vob);

    void    setViewerPos(const Tempest::Vec3& pos);

    void    resetTicks();
    void    tick(uint64_t ticks);

    void    preFrameUpdate(uint8_t fId);

  private:
    using Vertex = Resources::Vertex;

    struct ImplEmitter;
    struct Block final {
      uint64_t      timeTotal    = 0;

      size_t        offset       = 0;
      size_t        count        = 0;

      Tempest::Vec3 pos          = {};
      Tempest::Vec3 target       = {};
      Tempest::Vec3 direction[3] = {};
      bool          alive        = true;
      bool          hasTarget    = false;
      };

    struct ImplEmitter final {
      size_t        block        = size_t(-1);
      Tempest::Vec3 pos          = {};
      Tempest::Vec3 direction[3] = {};
      bool          alive        = true;
      bool          active       = true;
      };

    struct ParState final {
      uint16_t      life=0,maxLife=1;
      Tempest::Vec3 pos, dir;

      float         velocity=0;
      float         rotation=0;

      float         lifeTime() const;
      };

    struct Bucket final {
      Bucket(const ParticleFx &ow, PfxObjects* parent);

      ObjectsBucket::Item         item;
      Tempest::VertexBufferDyn<Vertex> vboGpu[Resources::MaxFramesInFlight];
      std::vector<Vertex>         vboCpu;

      std::vector<ParState>       particles;

      std::vector<ImplEmitter>    impl;
      std::vector<Block>          block;

      const ParticleFx*           owner=nullptr;
      PfxObjects*                 parent=nullptr;
      size_t                      blockSize=0;

      bool                        isEmpty() const;

      size_t                      allocBlock();
      void                        freeBlock(size_t& s);
      Block&                      getBlock(ImplEmitter& emitter);
      Block&                      getBlock(Emitter& emitter);

      size_t                      allocEmitter();
      bool                        shrink();

      void                        init    (Block& emitter, size_t particle);
      void                        finalize(size_t particle);
      void                        tick    (Block& sys, size_t particle, uint64_t dt);
      };

    struct SpriteEmitter {
      uint8_t                     visualCamAlign = 0;
      int32_t                     zBias          = 0;
      ZMath::float2               decalDim = {};
      std::unique_ptr<ParticleFx> pfx;
      };

    struct VboContext {
      float left[4] = {};
      float top [4] = {};
      float z   [4] = {};

      float leftA[4] = {};
      float topA [4] = {0,1,0};
      };

    static float                  randf();
    static float                  randf(float base, float var);
    Bucket&                       getBucket(const ParticleFx& decl);
    Bucket&                       getBucket(const Material& mat, const ZenLoad::zCVobData& vob);
    void                          tickSys    (Bucket& b, uint64_t dt);
    void                          tickSysEmit(Bucket& b, Block&  p, uint64_t emited);
    void                          buildVbo(Bucket& b, const VboContext& ctx);

    const SceneGlobals&           scene;
    VisualObjects&                visual;
    std::mutex                    sync;

    std::vector<std::unique_ptr<Bucket>> bucket;
    std::vector<SpriteEmitter>           spriteEmit;

    Tempest::Vec3                 viewePos={};

    static std::mt19937           rndEngine;
    uint64_t                      lastUpdate=0;
  };
