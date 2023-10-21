#pragma once

#include <Tempest/Texture2d>
#include <Tempest/VertexBuffer>
#include <Tempest/IndexBuffer>
#include <Tempest/UniformBuffer>

#include "bounds.h"
#include "material.h"
#include "resources.h"
#include "sceneglobals.h"
#include "instancestorage.h"
#include "graphics/mesh/submesh/staticmesh.h"
#include "graphics/mesh/submesh/animmesh.h"
#include "graphics/dynamic/visibilitygroup.h"
#include "graphics/dynamic/visibleset.h"

class Pose;
class RtScene;
class VisualObjects;

class ObjectsBucket {
  protected:
    struct UboMaterial;

    using Vertex  = Resources::Vertex;
    using VertexA = Resources::VertexA;

  public:
    enum {
      CAPACITY = VisibleSet::CAPACITY,
      };

    enum Type : uint8_t {
      LandscapeShadow,
      Landscape,
      Static,
      Movable,
      Animated,
      Pfx,
      Morph,
      };

    enum InstancingType : uint8_t {
      NoInstancing = 0,
      Normal,
      Aggressive, // allowed to draw disposed items
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
        void   setWind     (phoenix::animation_mode m, float intensity);
        void   startMMAnim (std::string_view anim, float intensity, uint64_t timeUntil);
        void   setPfxData  (const Tempest::StorageBuffer* ssbo, uint8_t fId);

        const Material&    material() const;
        const Bounds&      bounds()   const;
        Tempest::Matrix4x4 position() const;
        const StaticMesh*  mesh()     const;
        std::pair<uint32_t,uint32_t> meshSlice() const;

      private:
        ObjectsBucket* owner=nullptr;
        size_t         id=0;
      };

    ObjectsBucket(const Type type, const Material& mat, VisualObjects& owner, const SceneGlobals& scene,
                  const StaticMesh* stMesh, const AnimMesh* anim, const Tempest::StorageBuffer* desc);
    virtual ~ObjectsBucket();

    bool isCompatible(const Type type, const Material& mat,
                      const StaticMesh* st, const AnimMesh* ani, const Tempest::StorageBuffer* desc) const;

    static std::unique_ptr<ObjectsBucket> mkBucket(Type type, const Material& mat, VisualObjects& owner, const SceneGlobals& scene,
                                                   const StaticMesh* st, const AnimMesh* anim, const Tempest::StorageBuffer* desc);

    const Material&           material()      const;
    Type                      type()          const { return objType;        }
    InstancingType            hasInstancing() const { return instancingType; }
    const void*               meshPointer()   const;
    VisibleSet&               visibilitySet() { return visSet; };

    size_t                    size()          const { return valSz;      }
    size_t                    alloc(const StaticMesh& mesh, size_t iboOffset, size_t iboLen, const Bounds& bounds,
                                    const Material& mat);
    size_t                    alloc(const AnimMesh& mesh, size_t iboOffset, size_t iboLen,
                                    const InstanceStorage::Id& anim);
    size_t                    alloc(const Bounds& bounds);
    void                      free(const size_t objId);

    virtual void              prepareUniforms();
    virtual void              invalidateUbo(uint8_t fId);
    virtual void              fillTlas(RtScene& out);

    virtual void              preFrameUpdate(uint8_t fId);
    virtual void              drawHiZ    (Tempest::Encoder<Tempest::CommandBuffer> &cmd, uint8_t fId);
    void                      draw       (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);
    void                      drawGBuffer(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);
    void                      drawShadow (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, int layer);

  protected:
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
      L_MorphId  = 8,
      L_Morph    = 9,
      L_Pfx      = L_MorphId,
      L_SceneClr = 10,
      L_GDepth   = 11,
      L_HiZ      = 12,
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
      uint32_t  meshletBase        = 0;
      int32_t   meshletPerInstance = 0;
      uint32_t  firstInstance      = 0;
      uint32_t  instanceCount      = 0;
      float     fatness            = 0;
      float     padd[3]            = {};
      };

    struct UboPush : UboPushBase {
      MorphDesc morph[Resources::MAX_MORPH_LAYERS];
      };

    struct BucketDesc final {
      Tempest::Vec4  bbox[2];
      Tempest::Point texAniMapDirPeriod;
      float          bboxRadius = 0;
      float          waveMaxAmplitude = 0;
      float          alphaWeight = 1;
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
      const Tempest::StorageBuffer*         pfx[Resources::MaxFramesInFlight] = {};
      size_t                                iboOffset = 0;
      size_t                                iboLength = 0;
      Tempest::Matrix4x4                    pos;
      VisibilityGroup::Token                visibility;
      float                                 fatness = 0;
      phoenix::animation_mode               wind = phoenix::animation_mode::none;
      float                                 windIntensity = 0;
      uint64_t                              timeShift=0;

      const InstanceStorage::Id*            skiningAni = nullptr;
      MorphAnim                             morphAnim[Resources::MAX_MORPH_LAYERS];
      const Tempest::AccelerationStructure* blas = nullptr;

      bool                                  isValid = false;
      };

    using Bucket = Tempest::UniformBuffer<BucketDesc>;

    virtual Object&           implAlloc(const Bounds& bounds, const Material& mat);
    virtual void              postAlloc(Object& obj, size_t objId);
    virtual void              implFree(const size_t objId);
    Bucket                    allocBucketDesc(const Material& mat);

    void                      uboSetCommon  (Descriptors& v, const Material& mat, const Bucket& bucket);
    void                      uboSetSkeleton(Descriptors& v, uint8_t fId);
    void                      uboSetDynamic (Descriptors& v, Object& obj, uint8_t fId);

    void                      setObjMatrix(size_t i, const Tempest::Matrix4x4& m);
    void                      setBounds   (size_t i, const Bounds& b);
    void                      startMMAnim (size_t i, std::string_view anim, float intensity, uint64_t timeUntil);
    void                      setFatness  (size_t i, float f);
    void                      setWind     (size_t i, phoenix::animation_mode m, float intensity);
    virtual void              setPfxData  (size_t i, const Tempest::StorageBuffer* ssbo, uint8_t fId);

    static bool               isAnimated(const Material& mat);
    bool                      isForwardShading() const;
    bool                      isShadowmapRequired() const;
    bool                      isSceneInfoRequired() const;
    void                      updatePushBlock(UboPush& push, Object& v, uint32_t instance, uint32_t instanceCount);
    void                      reallocObjPositions();
    void                      invalidateInstancing();
    uint32_t                  applyInstancing(size_t& i, const size_t* index, size_t indSz) const;

    virtual Descriptors&      objUbo(size_t objId);
    virtual void              drawCommon(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId,
                                         const Tempest::RenderPipeline& shader, SceneGlobals::VisCamera c, bool isHiZPass);

    const Bounds&             bounds(size_t i) const;
    Tempest::Matrix4x4        position(size_t i) const;
    virtual const Material&   material(size_t i) const;
    std::pair<uint32_t,uint32_t> meshSlice(size_t i) const;

    static Type               sanitizeType(const Type t, const Material& mat, const StaticMesh* st);

    const Type                objType          = Type::Landscape;
    const StaticMesh*         staticMesh       = nullptr;
    const AnimMesh*           animMesh         = nullptr;
    const Tempest::StorageBuffer* instanceDesc = nullptr;

    VisualObjects&            owner;
    Descriptors               uboShared;
    VisibleSet                visSet;

    Object                    val[CAPACITY];
    size_t                    valSz = 0;
    InstanceStorage::Id       objPositions;

    bool                      useMeshlets         = false;
    bool                      textureInShadowPass = false;

    const Tempest::RenderPipeline* pMain      = nullptr;
    const Tempest::RenderPipeline* pShadow    = nullptr;

    const SceneGlobals&       scene;

    InstancingType            instancingType      = NoInstancing;
    bool                      useSharedUbo        = false;
    bool                      usePositionsSsbo    = false;
    bool                      windAnim            = false;

  private:
    Material                  mat;
    Bucket                    bucketShared;
  };


class ObjectsBucketDyn : public ObjectsBucket {
  public:
    ObjectsBucketDyn(const Type type, const Material& mat, VisualObjects& owner, const SceneGlobals& scene,
                     const StaticMesh* st, const AnimMesh* anim, const Tempest::StorageBuffer* desc);

    void         preFrameUpdate(uint8_t fId) override;

  private:
    Object&      implAlloc(const Bounds& bounds, const Material& mat) override;
    void         implFree (const size_t objId) override;

    Descriptors& objUbo(size_t objId) override;
    void         setPfxData  (size_t i, const Tempest::StorageBuffer* ssbo, uint8_t fId) override;
    const Material& material(size_t i) const override;

    void         prepareUniforms() override;
    void         invalidateUbo(uint8_t fId) override;
    void         fillTlas(RtScene& out) override;

    void         invalidateDyn();

    void         drawCommon(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId,
                            const Tempest::RenderPipeline& shader, SceneGlobals::VisCamera c, bool isHiZPass) override;
    void         drawHiZ   (Tempest::Encoder<Tempest::CommandBuffer> &cmd, uint8_t fId) override;

    Descriptors  uboObj   [CAPACITY];

    Material     mat      [CAPACITY];
    Bucket       bucketObj[CAPACITY];
    bool         hasDynMaterials = false;

    const Tempest::RenderPipeline* pHiZ = nullptr;
    Descriptors                    uboHiZ;
  };
