#pragma once

#include <Tempest/DescriptorSet>
#include <Tempest/RenderPipeline>
#include <Tempest/StorageBuffer>
#include <Tempest/Texture2d>

#include <cstddef>
#include <utility>

#include "graphics/mesh/submesh/packedmesh.h"
#include "graphics/bounds.h"
#include "graphics/material.h"
#include "graphics/sceneglobals.h"
#include "graphics/instancestorage.h"

class StaticMesh;
class AnimMesh;
class VisualObjects;
class Camera;

class DrawStorage {
  public:
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
        Item(DrawStorage& owner,size_t id)
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

        bool     isEmpty() const { return owner==nullptr; }

        void     setObjMatrix(const Tempest::Matrix4x4& mt);
        void     setAsGhost  (bool g);
        void     setFatness  (float f);
        void     setWind     (phoenix::animation_mode m, float intensity);
        void     startMMAnim (std::string_view anim, float intensity, uint64_t timeUntil);
        void     setPfxData  (const Tempest::StorageBuffer* ssbo, uint8_t fId);

        const Material&    material() const;
        const Bounds&      bounds()   const;
        Tempest::Matrix4x4 position() const;
        const StaticMesh*  mesh()     const;
        std::pair<uint32_t,uint32_t> meshSlice() const;

      private:
        DrawStorage* owner = nullptr;
        size_t       id    = 0;
      };

    DrawStorage(VisualObjects& owner, const SceneGlobals& globals);
    ~DrawStorage();

    Item alloc(const AnimMesh& mesh, const Material& mat, const InstanceStorage::Id& anim,
               size_t iboOff, size_t iboLen, Type type);
    Item alloc(const StaticMesh& mesh, const Material& mat,
               size_t iboOff, size_t iboLen, Type type);
    Item alloc(const StaticMesh& mesh, const Material& mat,
               size_t iboOff, size_t iboLen, const PackedMesh::Cluster* cluster, Type type);

    void dbgDraw(Tempest::Painter& p, Tempest::Vec2 wsz);

    bool commit();
    void prepareUniforms();
    void invalidateUbo();

    void visibilityPass (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);
    void drawGBuffer    (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);
    void drawShadow     (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, int layer);

  private:
    enum TaskLinkpackage : uint8_t {
      T_Scene    = 0,
      T_Indirect = 1,
      T_Clusters = 2,
      T_Payload  = 3,
      T_Instance = 4,
      T_Bucket   = 5,
      T_HiZ      = 6,
      };

    enum UboLinkpackage : uint8_t {
      L_Scene    = 0,
      L_Instance = 1,
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
      L_HiZ      = 12, //NOTE: remove it
      L_SkyLut   = 13,
      L_Payload  = 14,
      L_Sampler  = 15,
      };

    struct Cluster final {
      Tempest::Vec3 pos;
      float         r            = 0;
      uint16_t      commandId    = 0;
      uint16_t      bucketId     = 0;
      uint32_t      firstMeshlet = 0;
      uint32_t      meshletCount = 0;
      uint32_t      instanceId   = 0;
      };

    struct Object {
      bool     isEmpty() const { return cmdId==uint16_t(-1); }

      Tempest::Matrix4x4  pos;
      InstanceStorage::Id objInstance;
      InstanceStorage::Id objMorphAnim;
      Type                type      = Type::Landscape;
      uint32_t            iboOff    = 0;
      uint32_t            iboLen    = 0;
      uint32_t            animPtr   = 0;
      uint16_t            bucketId  = 0;
      uint16_t            cmdId     = uint16_t(-1);
      uint32_t            clusterId = 0;
      float               fatness   = 0;
      bool                isGhost   = false;
      };

    struct MorphDesc final {
      uint32_t indexOffset = 0;
      uint32_t sample0     = 0;
      uint32_t sample1     = 0;
      uint16_t alpha       = 0;
      uint16_t intensity   = 0;
      };

    struct MorphData {
      MorphDesc morph[Resources::MAX_MORPH_LAYERS];
      };

    struct InstanceDesc {
      void     setPosition(const Tempest::Matrix4x4& m);
      float    pos[4][3] = {};
      float    fatness   = 0;
      uint32_t animPtr   = 0;
      uint32_t padd0     = {};
      uint32_t padd1     = {};
      };

    struct IndirectCmd {
      uint32_t vertexCount   = 0;
      uint32_t instanceCount = 0;
      uint32_t firstVertex   = 0;
      uint32_t firstInstance = 0;
      uint32_t writeOffset   = 0;
      };

    struct Bucket {
      const StaticMesh* staticMesh = nullptr;
      const AnimMesh*   animMesh   = nullptr;
      Material          mat;
      };

    struct BucketGpu final {
      Tempest::Vec4  bbox[2];
      Tempest::Point texAniMapDirPeriod;
      float          bboxRadius       = 0;
      float          waveMaxAmplitude = 0;
      float          alphaWeight      = 1;
      float          envMapping       = 0;
      uint32_t       padd[2]          = {};
      };

    struct TaskCmd {
      SceneGlobals::VisCamera        viewport = SceneGlobals::V_Main;
      Tempest::DescriptorSet         desc;
      };

    struct DrawCmd {
      const Tempest::RenderPipeline* psoColor     = nullptr;
      const Tempest::RenderPipeline* psoDepth     = nullptr;
      uint32_t                       bucketId     = 0; // bindfull only
      Type                           type         = Type::Landscape;

      Tempest::DescriptorSet         desc[SceneGlobals::V_Count];
      Material::AlphaFunc            alpha        = Material::Solid;
      uint32_t                       firstPayload = 0;
      uint32_t                       maxPayload   = 0;
      };

    struct View {
      Tempest::DescriptorSet         descInit;
      Tempest::StorageBuffer         visClusters, indirectCmd;
      };

    void                           free(size_t id);
    void                           updateInstance(size_t id);

    bool                           commitCommands();
    bool                           commitBuckets();

    size_t                         implAlloc();
    uint16_t                       bucketId (const Material& m, const StaticMesh& mesh);
    uint16_t                       bucketId (const Material& m, const AnimMesh&   mesh);
    uint16_t                       commandId(const Material& m, Type type, uint32_t bucketId);
    uint32_t                       clusterId(const PackedMesh::Cluster* cluster, size_t firstMeshlet, size_t meshletCount, uint16_t bucketId, uint16_t commandId);
    uint32_t                       clusterId(const Bucket&  bucket,  size_t firstMeshlet, size_t meshletCount, uint16_t bucketId, uint16_t commandId);

    void                           dbgDraw(Tempest::Painter& p, Tempest::Vec2 wsz, const Camera& cam, const Cluster& c);
    void                           dbgDrawBBox(Tempest::Painter& p, Tempest::Vec2 wsz, const Camera& cam, const Cluster& c);

    VisualObjects&                 owner;
    const SceneGlobals&            scene;
    bool                           commited = false;
    std::vector<TaskCmd>           tasks;

    std::vector<Object>            objects;

    std::vector<Bucket>            buckets;
    Tempest::StorageBuffer         bucketsGpu;
    bool                           bucketsDurtyBit = false;

    size_t                         totalPayload = 0;
    std::vector<Cluster>           clusters;
    Tempest::StorageBuffer         clustersGpu;

    std::vector<DrawCmd>           cmd;
    std::vector<DrawCmd*>          ord;
    bool                           cmdDurtyBit = false;
    View                           views[SceneGlobals::V_Count];
  };
