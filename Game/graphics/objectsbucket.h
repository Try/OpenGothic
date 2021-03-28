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
#include "skeletalstorage.h"
#include "ubostorage.h"
#include "graphics/mesh/protomesh.h"
#include "graphics/dynamic/visibilitygroup.h"

class RendererStorage;
class Pose;
class VisualObjects;

class ObjectsBucket final {
  private:
    struct UboMaterial;

    using Vertex  = Resources::Vertex;
    using VertexA = Resources::VertexA;

  public:
    enum {
      CAPACITY     = 128,
      };

    enum Type : uint8_t {
      Static,
      Movable,
      Animated,
      Morph,
      };

    enum VboType : uint8_t {
      NoVbo,
      VboVertex,
      VboVertexA,
      VboMorph,
      VboMorpthGpu,
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

        void   setObjMatrix (const Tempest::Matrix4x4& mt);
        void   setPose      (const Pose&                p);
        void   setAsGhost   (bool g);

        const Bounds& bounds() const;

        void   draw(Tempest::Encoder<Tempest::CommandBuffer>& p, uint8_t fId) const;

      private:
        ObjectsBucket* owner=nullptr;
        size_t         id=0;
      };

    class Storage final {
      public:
        Storage(Tempest::Device& device):ani(device){}
        SkeletalStorage         ani;
        UboStorage<UboMaterial> mat;
        bool                    commitUbo(Tempest::Device &device, uint8_t fId);
      };

    ObjectsBucket(const Material& mat, const std::vector<ProtoMesh::Animation>& anim, size_t boneCount, VisualObjects& owner, const SceneGlobals& scene, Storage& storage, const Type type);
    ~ObjectsBucket();

    const Material&           material()  const;
    Type                      type()      const { return shaderType; }
    size_t                    size()      const { return valSz;      }
    size_t                    boneCount() const { return boneCnt;    }
    const std::vector<ProtoMesh::Animation>* morph() const { return morphAnim;  }

    size_t                    avgPoligons() const { return polySz; }

    size_t                    alloc(const Tempest::VertexBuffer<Vertex>  &vbo,
                                    const Tempest::IndexBuffer<uint32_t> &ibo,
                                    size_t iboOffset, size_t iboLen,
                                    const Bounds& bounds);
    size_t                    alloc(const Tempest::VertexBuffer<VertexA> &vbo,
                                    const Tempest::IndexBuffer<uint32_t> &ibo,
                                    size_t iboOffset, size_t iboLen,
                                    const Bounds& bounds);
    size_t                    alloc(const Tempest::VertexBuffer<Vertex>* vbo[],
                                    const Bounds& bounds);
    void                      free(const size_t objId);

    void                      setupUbo();
    void                      invalidateUbo();

    void                      preFrameUpdate(uint8_t fId);
    void                      draw       (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);
    void                      drawGBuffer(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);
    void                      drawShadow (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, int layer=0);

    void                      draw       (size_t id, Tempest::Encoder<Tempest::CommandBuffer>& p, uint8_t fId);

  private:
    struct ShLight final {
      Tempest::Vec3 pos;
      float         padding=0;
      Tempest::Vec3 color;
      float         range=0;
      };

    struct UboPush final {
      Tempest::Matrix4x4 pos;
      int32_t            samplesPerFrame = 0;
      int32_t            morphFrame[2] = {};
      float              morphAlpha;
      };

    struct UboMaterial final {
      Tempest::Vec2 texAniMapDir;
      };

    struct Descriptors final {
      Tempest::Uniforms       ubo  [Resources::MaxFramesInFlight][VisibilityGroup::V_Count];
      //Tempest::Uniforms       uboSh[Resources::MaxFramesInFlight][Resources::ShadowLayers];
      bool                    uboIsReady[Resources::MaxFramesInFlight] = {};

      void                    invalidate();
      void                    alloc(ObjectsBucket& owner);
      };

    struct Object final {
      VboType                               vboType = VboType::NoVbo;
      const Tempest::VertexBuffer<Vertex>*  vbo     = nullptr;
      const Tempest::VertexBuffer<Vertex>*  vboM[Resources::MaxFramesInFlight] = {};
      const Tempest::VertexBuffer<VertexA>* vboA    = nullptr;
      const Tempest::IndexBuffer<uint32_t>* ibo     = nullptr;
      size_t                                iboOffset = 0;
      size_t                                iboLength = 0;
      Tempest::Matrix4x4                    pos;
      VisibilityGroup::Token                visibility;

      Descriptors                           ubo;
      size_t                                storageAni = size_t(-1);
      uint64_t                              timeShift=0;

      size_t                                morphAnimId = 0;

      bool                                  isValid() const { return vboType!=VboType::NoVbo; }
      };

    Object& implAlloc(const VboType type, const Bounds& bounds);
    void    uboSetCommon (Descriptors& v);
    void    uboSetDynamic(Object& v, uint8_t fId);

    bool    groupVisibility(const Frustrum& f);

    void    setObjMatrix(size_t i,const Tempest::Matrix4x4& m);
    void    setPose     (size_t i,const Pose& sk);
    void    setBounds   (size_t i,const Bounds& b);

    bool    isSceneInfoRequired() const;
    void    updatePushBlock(UboPush& push, Object& v);

    void    drawCommon(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, const Tempest::RenderPipeline& shader, VisibilityGroup::VisCamera c);

    const Bounds& bounds(size_t i) const;

    VisualObjects&            owner;
    Descriptors               uboShared;

    Object                    val  [CAPACITY];
    size_t                    valSz=0;
    size_t                    boneCnt=0;
    size_t                    valLast=0;
    size_t                    polySz=0;
    size_t                    polyAvg=0;

    const SceneGlobals&       scene;
    Storage&                  storage;
    Material                  mat;
    const std::vector<ProtoMesh::Animation>* morphAnim = nullptr;

    Tempest::UniformBuffer<UboMaterial> uboMat[Resources::MaxFramesInFlight];

    const Type                shaderType;
    bool                      useSharedUbo=false;
    bool                      textureInShadowPass=false;

    Bounds                    allBounds;

    const Tempest::RenderPipeline* pMain    = nullptr;
    const Tempest::RenderPipeline* pGbuffer = nullptr;
    const Tempest::RenderPipeline* pShadow  = nullptr;
  };

