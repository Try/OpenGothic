#pragma once

#include <Tempest/Matrix4x4>
#include <Tempest/UniformBuffer>

#include <memory>
#include <list>
#include <random>

#include "resources.h"
#include "ubochain.h"

class RendererStorage;
class ParticleFx;
class Light;
class Pose;

class PfxObjects final {
  private:
    struct Bucket;

  public:
    PfxObjects(const RendererStorage& storage);

    class Emitter final {
      public:
        Emitter()=default;
        ~Emitter();
        Emitter(Emitter&&);
        Emitter& operator=(Emitter&& b);

        Emitter(const Emitter&)=delete;

        bool   isEmpty() const { return bucket==nullptr; }
        void   setPosition (float x,float y,float z);
        void   setObjMatrix(const Tempest::Matrix4x4& mt);
        void   setActive(bool act);

      private:
        Emitter(Bucket &b,size_t id);

        Bucket*         bucket = nullptr;
        size_t          id     = size_t(-1);

      friend class PfxObjects;
      };

    Emitter get(const ParticleFx& decl);

    void    setModelView(const Tempest::Matrix4x4 &m, const Tempest::Matrix4x4 &shadow);
    void    setLight(const Light &l, const Tempest::Vec3 &ambient);
    void    setViewerPos(const Tempest::Vec3& pos);

    bool    needToUpdateCommands(uint8_t fId) const;
    void    setAsUpdated(uint8_t fId);

    void    resetTicks();
    void    tick(uint64_t ticks);

    void    updateUbo(uint8_t frameId);
    void    commitUbo(uint8_t frameId, const Tempest::Texture2d& shadowMap);
    void    draw     (Tempest::Encoder<Tempest::CommandBuffer> &cmd, uint32_t imgId);

  private:
    using Vertex = Resources::Vertex;

    //FIXME: copypaste
    struct UboGlobal final {
      std::array<float,3>           lightDir={{0,0,1}};
      float                         padding=0;
      Tempest::Matrix4x4            modelView;
      Tempest::Matrix4x4            shadowView;
      std::array<float,4>           lightAmb={{0,0,0}};
      std::array<float,4>           lightCl ={{1,1,1}};
      };

    struct PerFrame final {
      Tempest::Uniforms                ubo;
      Tempest::VertexBufferDyn<Vertex> vbo;
      };

    struct ImplEmitter;
    struct Block final {
      uint64_t      timeTotal=0;
      uint64_t      emited=0;

      size_t        offset=0;
      size_t        count=0;

      Tempest::Vec3 pos={};
      bool          alive  = true;
      };

    struct ImplEmitter final {
      size_t        block  = size_t(-1);
      Tempest::Vec3 pos    = {};
      bool          alive  = true;
      bool          active = true;
      };

    struct ParState final {
      uint16_t      life=0,maxLife=1;
      Tempest::Vec3 pos, dir;

      float         rotation=0.f, drotation=0.f;

      float         lifeTime() const;
      };

    struct Bucket final {
      Bucket(const RendererStorage& storage,const ParticleFx &ow,PfxObjects* parent);
      std::unique_ptr<PerFrame[]> pf;

      std::vector<Vertex>         vbo;
      std::vector<ParState>       particles;

      std::vector<ImplEmitter>    impl;
      std::vector<Block>          block;

      const ParticleFx*           owner=nullptr;
      PfxObjects*                 parent=nullptr;
      size_t                      blockSize=0;

      size_t                      allocBlock();
      void                        freeBlock(size_t& s);
      Block&                      getBlock(ImplEmitter& emitter);
      Block&                      getBlock(Emitter& emitter);

      size_t                      allocEmitter();
      bool                        shrink();

      void                        init    (size_t particle);
      void                        finalize(size_t particle);
      };

    static float                  randf();
    Bucket&                       getBucket(const ParticleFx& decl);
    void                          tickSys    (Bucket& b, uint64_t dt);
    void                          tickSys    (Bucket& b, Block&  p, uint64_t dt);
    void                          tickSysEmit(Bucket& b, Block&  p, uint64_t emited);
    void                          buildVbo(Bucket& b);

    void                          invalidateCmd();

    const RendererStorage&        storage;
    std::mutex                    sync;
    std::list<Bucket>             bucket;

    Tempest::Vec3                 viewePos={};

    static std::mt19937           rndEngine;
    UboChain<UboGlobal,void>      uboGlobalPf;
    UboGlobal                     uboGlobal;
    uint64_t                      lastUpdate=0;
    std::vector<bool>             updateCmd;
  };
