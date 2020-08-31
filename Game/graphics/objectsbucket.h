#pragma once

#include <Tempest/Texture2d>
#include <Tempest/VertexBuffer>
#include <Tempest/IndexBuffer>
#include <Tempest/UniformBuffer>
#include <Tempest/UniformsLayout>

#include "bounds.h"
#include "material.h"
#include "resources.h"
#include "sceneglobals.h"
#include "ubostorage.h"

class RendererStorage;
class Pose;
class Painter3d;

class ObjectsBucket final {
  private:
    struct UboAnim;
    struct UboMaterial;
    using Vertex  = Resources::Vertex;
    using VertexA = Resources::VertexA;

    enum {
      LIGHT_BLOCK  = 2,
      MAX_LIGHT    = 64,
      };

  public:
    enum {
      CAPACITY     = 128,
      };

    enum Type : uint8_t {
      Static,
      Movable,
      Animated,
      };

    enum VboType : uint8_t {
      NoVbo,
      VboVertex,
      VboVertexA,
      VboMorph,
      };

    class Item final {
      public:
        Item()=default;
        Item(ObjectsBucket& owner,size_t id)
          :owner(&owner),id(id){}
        Item(Item&& obj):owner(obj.owner),id(obj.id){
          obj.owner=nullptr;
          obj.id   =0;
          }
        Item& operator=(Item&& obj) {
          std::swap(obj.owner,owner);
          std::swap(obj.id,   id);
          return *this;
          }
        ~Item() {
          if(owner)
            owner->free(this->id);
          }

        bool   isEmpty() const { return owner==nullptr; }

        void   setObjMatrix(const Tempest::Matrix4x4& mt);
        void   setPose     (const Pose&                p);
        void   setBounds   (const Bounds&           bbox);

        void   draw(Tempest::Encoder<Tempest::CommandBuffer>& p, uint8_t fId) const;

      private:
        ObjectsBucket* owner=nullptr;
        size_t         id=0;
      };

    class Storage final {
      public:
        UboStorage<UboAnim>     ani;
        UboStorage<UboMaterial> mat;
        bool                    commitUbo(Tempest::Device &device, uint8_t fId);
      };

    ObjectsBucket(const Material& mat, const SceneGlobals& scene, Storage& storage, const Type type);
    ~ObjectsBucket();

    const Material&           material() const;
    Type                      type()     const { return shaderType; }
    size_t                    size()     const { return valSz; }

    size_t                    avgPoligons() const { return polySz; }

    size_t                    alloc(const Tempest::VertexBuffer<Vertex>  &vbo,
                                    const Tempest::IndexBuffer<uint32_t> &ibo,
                                    const Bounds& bounds);
    size_t                    alloc(const Tempest::VertexBuffer<VertexA> &vbo,
                                    const Tempest::IndexBuffer<uint32_t> &ibo,
                                    const Bounds& bounds);
    size_t                    alloc(const Tempest::VertexBuffer<Vertex>* vbo[],
                                    const Bounds& bounds);
    void                      free(const size_t objId);

    void                      setupUbo();
    void                      invalidateUbo();

    void                      preFrameUpdate(uint8_t fId);
    void                      visibilityPass(Painter3d& p);
    void                      visibilityPassAnd(Painter3d& p);
    void                      draw      (Tempest::Encoder<Tempest::CommandBuffer>& painter, uint8_t fId);
    void                      drawLight (Tempest::Encoder<Tempest::CommandBuffer>& painter, uint8_t fId);
    void                      drawShadow(Tempest::Encoder<Tempest::CommandBuffer>& painter, uint8_t fId, int layer=0);
    void                      draw      (size_t id, Tempest::Encoder<Tempest::CommandBuffer>& p, uint8_t fId);

  private:
    struct ShLight final {
      Tempest::Vec3 pos;
      float         padding=0;
      Tempest::Vec3 color;
      float         range=0;
      };

    struct UboPush final {
      Tempest::Matrix4x4 pos;
      ShLight            light[LIGHT_BLOCK];
      };

    struct UboAnim final {
      Tempest::Matrix4x4 skel[Resources::MAX_NUM_SKELETAL_NODES];
      };

    struct UboMaterial final {
      Tempest::Vec2 texAniMapDir;
      };

    struct Descriptors final {
      Tempest::Uniforms       ubo  [Resources::MaxFramesInFlight];
      Tempest::Uniforms       uboSh[Resources::MaxFramesInFlight][Resources::ShadowLayers];

      uint8_t                 uboBit  [Resources::MaxFramesInFlight]={};
      uint8_t                 uboBitSh[Resources::MaxFramesInFlight][Resources::ShadowLayers]={};

      void                    invalidate();
      void                    alloc(ObjectsBucket& owner);
      };

    struct Object final {
      VboType                               vboType = VboType::NoVbo;
      const Tempest::VertexBuffer<Vertex>*  vbo     = nullptr;
      const Tempest::VertexBuffer<Vertex>*  vboM[Resources::MaxFramesInFlight] = {};
      const Tempest::VertexBuffer<VertexA>* vboA    = nullptr;
      const Tempest::IndexBuffer<uint32_t>* ibo     = nullptr;
      Bounds                                bounds;
      Tempest::Matrix4x4                    pos;

      Descriptors                           ubo;
      size_t                                storageAni = size_t(-1);

      const Light*                          light[MAX_LIGHT] = {};
      size_t                                lightCnt=0;
      int                                   lightCacheKey[3]={};

      size_t                                texAnim=0;
      uint64_t                              timeShift=0;

      bool                                  isValid() const { return vboType!=VboType::NoVbo; }
      };

    Descriptors               uboShared;

    Object                    val  [CAPACITY];
    size_t                    valSz=0;
    size_t                    valLast=0;
    Object*                   index[CAPACITY] = {};
    size_t                    indexSz=0;
    size_t                    polySz=0;
    size_t                    polyAvg=0;

    const SceneGlobals&       scene;
    Storage&                  storage;
    Material                  mat;

    Tempest::UniformBuffer<UboMaterial> uboMat[Resources::MaxFramesInFlight];

    const Type                shaderType;
    bool                      useSharedUbo=false;
    bool                      textureInShadowPass=false;

    Bounds                    allBounds;

    const Tempest::RenderPipeline* pMain   = nullptr;
    const Tempest::RenderPipeline* pLight  = nullptr;
    const Tempest::RenderPipeline* pShadow = nullptr;

    Object& implAlloc(const VboType type, const Bounds& bounds);
    void    uboSetCommon(Descriptors& v);
    bool    groupVisibility(Painter3d& p);

    void    setObjMatrix(size_t i,const Tempest::Matrix4x4& m);
    void    setPose     (size_t i,const Pose& sk);
    void    setBounds   (size_t i,const Bounds& b);

    void    setupLights (Object& val, bool noCache);

    void    setAnim(Object& val, Tempest::Uniforms& ubo);
    template<class T>
    void    setUbo(uint8_t& bit, Tempest::Uniforms& ubo, uint8_t layoutBind,
                   const Tempest::UniformBuffer<T>& vbuf,size_t offset,size_t size);
    void    setUbo(uint8_t& bit, Tempest::Uniforms& ubo, uint8_t layoutBind,
                   const Tempest::Texture2d&  tex, const Tempest::Sampler2d& smp = Tempest::Sampler2d::anisotrophy());
  };

