#pragma once

#include <Tempest/Matrix4x4>
#include <Tempest/UniformBuffer>

#include <memory>
#include <list>

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

        void setPosition(float x,float y,float z);

        //void   setSkeleton(const Skeleton* sk,const char* defBone=nullptr);
        //void   setSkeleton(const Pose&      p,const Tempest::Matrix4x4& obj);

      private:
        Emitter(Bucket &b,size_t id);

        Bucket* bucket=nullptr;
        size_t  id=0;

      friend class PfxObjects;
      };

    Emitter get(const ParticleFx& decl);

    void    setModelView(const Tempest::Matrix4x4 &m, const Tempest::Matrix4x4 &shadow);
    void    setLight(const Light &l, const Tempest::Vec3 &ambient);

    bool    needToUpdateCommands() const;
    void    setAsUpdated();

    void    updateUbo(uint32_t imgId);
    void    commitUbo(uint32_t imgId, const Tempest::Texture2d &shadowMap);
    void    draw     (Tempest::CommandBuffer &cmd, uint32_t imgId);

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

    struct ImplEmitter {
      float pos[3]={};
      bool  enable=true;
      };

    struct Bucket final {
      Bucket(const RendererStorage& storage,const ParticleFx &ow);
      std::unique_ptr<PerFrame[]> pf;

      std::vector<Vertex>         vbo;
      std::vector<ImplEmitter>    impl;

      const ParticleFx*           owner=nullptr;
      };

    Bucket&                getBucket(const ParticleFx& decl);
    void                   tickSys(Bucket& b);

    const RendererStorage&   storage;
    std::list<Bucket>        bucket;

    UboChain<UboGlobal,void> uboGlobalPf;
    UboGlobal                uboGlobal;
    bool                     updateCmd=false;
  };

