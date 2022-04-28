#pragma once

#include <Tempest/Texture2d>
#include <Tempest/VertexBuffer>
#include <Tempest/IndexBuffer>
#include <Tempest/UniformBuffer>

#include "bounds.h"
#include "material.h"
#include "resources.h"
#include "sceneglobals.h"
#include "skeletalstorage.h"
#include "ubostorage.h"
#include "graphics/mesh/protomesh.h"
#include "graphics/dynamic/visibilitygroup.h"
#include "graphics/dynamic/visibleset.h"
#include "graphics/skeletalstorage.h"

class Pose;
class VisualObjects;

class ObjectsBucket {
  protected:
    struct UboMaterial;

    using Vertex  = Resources::Vertex;
    using VertexA = Resources::VertexA;

  public:
    enum {
      CAPACITY     = VisibleSet::CAPACITY,
      };

    enum Type : uint8_t {
      Landscape,
      Static,
      Movable,
      Animated,
      Pfx,
      Morph,
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
        void   setAsGhost  (bool g);
        void   setFatness  (float f);
        void   setWind     (ZenLoad::AnimMode m, float intensity);
        void   startMMAnim (std::string_view anim, float intensity, uint64_t timeUntil);

        const Bounds& bounds() const;

        void   draw(Tempest::Encoder<Tempest::CommandBuffer>& p, uint8_t fId) const;

      private:
        ObjectsBucket* owner=nullptr;
        size_t         id=0;
      };

    ObjectsBucket(const Material& mat, const ProtoMesh* anim, VisualObjects& owner, const SceneGlobals& scene, const Type type);
    virtual ~ObjectsBucket();

    static std::unique_ptr<ObjectsBucket> mkBucket(const Material& mat, const ProtoMesh* anim, VisualObjects& owner,
                                                   const SceneGlobals& scene, const Type type);

    const Material&           material()  const;
    const std::vector<ProtoMesh::Animation>* morph() const { return morphAnim==nullptr ? nullptr : &morphAnim->morph;  }

    Type                      type()      const { return objType;    }

    size_t                    size()      const { return valSz;      }
    size_t                    alloc(const Tempest::VertexBuffer<Vertex>  &vbo,
                                    const Tempest::IndexBuffer<uint32_t> &ibo,
                                    const Tempest::AccelerationStructure* blas,
                                    size_t iboOffset, size_t iboLen,
                                    const Bounds& bounds);
    size_t                    alloc(const Tempest::VertexBuffer<VertexA> &vbo,
                                    const Tempest::IndexBuffer<uint32_t> &ibo,
                                    size_t iboOffset, size_t iboLen,
                                    const SkeletalStorage::AnimationId& anim,
                                    const Bounds& bounds);
    size_t                    alloc(const Tempest::VertexBuffer<Vertex>* vbo[],
                                    const Bounds& bounds);
    void                      free(const size_t objId);

    virtual void              setupUbo();
    void                      invalidateUbo();
    void                      resetVis();
    void                      fillTlas(std::vector<Tempest::RtInstance>& inst);

    virtual void              preFrameUpdate(uint8_t fId);
    void                      draw       (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);
    void                      drawGBuffer(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);
    void                      drawShadow (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, int layer=0);
    void                      draw       (size_t id, Tempest::Encoder<Tempest::CommandBuffer>& p, uint8_t fId);

  protected:
    enum VboType : uint8_t {
      NoVbo,
      VboVertex,
      VboVertexA,
      VboMorph,
      VboMorphGpu,
      };

    enum UboLinkpackage : uint8_t {
      L_Diffuse  = 0,
      L_Shadow0  = 1,
      L_Shadow1  = 2,
      L_Scene    = 3,
      L_Skinning = 4,
      L_Material = 5,
      L_GDiffuse = 6,
      L_GDepth   = 7,
      L_MorphId  = 8,
      L_Morph    = 9,
      };

    struct ShLight final {
      Tempest::Vec3 pos;
      float         padding=0;
      Tempest::Vec3 color;
      float         range=0;
      };

    struct MorphDesc final {
      uint32_t indexOffset;
      uint32_t sample0;
      uint32_t sample1;
      uint16_t alpha;
      uint16_t intensity;
      };

    struct UboPushBase {
      Tempest::Matrix4x4 pos;
      float              fatness = 0;
      };

    struct UboPush : UboPushBase {
      float              padd[3] = {};
      MorphDesc          morph[Resources::MAX_MORPH_LAYERS];
      };

    struct UboMaterial final {
      Tempest::Vec2 texAniMapDir;
      float         waveAnim = 0;
      float         waveMaxAmplitude = 0;
      };

    struct Descriptors final {
      Tempest::DescriptorSet  ubo[Resources::MaxFramesInFlight][SceneGlobals::V_Count];
      void                    alloc(ObjectsBucket& owner);
      };

    struct MorphAnim {
      size_t   id        = 0;
      uint64_t timeStart = 0;
      uint64_t timeUntil = 0;
      float    intensity = 0;
      };

    struct Object final {
      const Tempest::VertexBuffer<Vertex>*  vbo     = nullptr;
      const Tempest::VertexBuffer<Vertex>*  vboM[Resources::MaxFramesInFlight] = {};
      const Tempest::VertexBuffer<VertexA>* vboA    = nullptr;
      const Tempest::IndexBuffer<uint32_t>* ibo     = nullptr;
      size_t                                iboOffset = 0;
      size_t                                iboLength = 0;
      Tempest::Matrix4x4                    pos;
      VisibilityGroup::Token                visibility;
      float                                 fatness = 0;
      ZenLoad::AnimMode                     wind = ZenLoad::AnimMode::NONE;
      float                                 windIntensity = 0;
      uint64_t                              timeShift=0;

      const SkeletalStorage::AnimationId*   skiningAni = nullptr;
      MorphAnim                             morphAnim[Resources::MAX_MORPH_LAYERS];
      const Tempest::AccelerationStructure* blas = nullptr;

      bool                                  isValid = false;
      };

    virtual Object& implAlloc(const VboType type, const Bounds& bounds, const SkeletalStorage::AnimationId* anim);
    virtual void    implFree(const size_t objId);

    void            uboSetCommon (Descriptors& v);
    void            uboSetDynamic(Descriptors& v, Object& obj, uint8_t fId);

    void            setObjMatrix(size_t i, const Tempest::Matrix4x4& m);
    void            setBounds   (size_t i, const Bounds& b);
    void            startMMAnim (size_t i, std::string_view anim, float intensity, uint64_t timeUntil);
    void            setFatness  (size_t i, float f);
    void            setWind     (size_t i, ZenLoad::AnimMode m, float intensity);

    bool            isSceneInfoRequired() const;
    void            updatePushBlock(UboPush& push, Object& v);

    virtual Descriptors& objUbo(size_t objId);
    virtual void         drawCommon(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, const Tempest::RenderPipeline& shader, SceneGlobals::VisCamera c);

    const Bounds& bounds(size_t i) const;

    static VboType toVboType(const Type t);

    const Type                objType   = Type::Landscape;
    const VboType             vboType   = VboType::NoVbo;
    const ProtoMesh*          morphAnim = nullptr;

    VisualObjects&            owner;
    Descriptors               uboShared;
    VisibleSet                visSet;

    Object                    val[CAPACITY];
    size_t                    valSz = 0;

  private:
    const SceneGlobals&       scene;
    Material                  mat;

    Tempest::UniformBuffer<UboMaterial> uboMat[Resources::MaxFramesInFlight];

    bool                      useSharedUbo        = false;
    bool                      textureInShadowPass = false;

    const Tempest::RenderPipeline* pMain    = nullptr;
    const Tempest::RenderPipeline* pGbuffer = nullptr;
    const Tempest::RenderPipeline* pShadow  = nullptr;
  };


class ObjectsBucketLnd : public ObjectsBucket {
  public:
    ObjectsBucketLnd(const Material& mat, const ProtoMesh* anim, VisualObjects& owner, const SceneGlobals& scene, const Type type)
      :ObjectsBucket(mat,anim,owner,scene,type) {
      }

  private:
    void    drawCommon(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId,
                       const Tempest::RenderPipeline& shader, SceneGlobals::VisCamera c) override;
  };

class ObjectsBucketDyn : public ObjectsBucket {
  public:
    ObjectsBucketDyn(const Material& mat, const ProtoMesh* anim, VisualObjects& owner, const SceneGlobals& scene, const Type type)
      :ObjectsBucket(mat,anim,owner,scene,type) {
      }

    void preFrameUpdate(uint8_t fId) override;

  private:
    Object& implAlloc(const VboType type, const Bounds& bounds, const SkeletalStorage::AnimationId* anim) override;
    void    implFree (const size_t objId) override;

    Descriptors& objUbo(size_t objId) override;

    void    setupUbo() override;

    void    drawCommon(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId,
                       const Tempest::RenderPipeline& shader, SceneGlobals::VisCamera c) override;

    Descriptors uboObj[CAPACITY];
  };
