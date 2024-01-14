#pragma once

#include <Tempest/DescriptorSet>
#include <Tempest/RenderPipeline>
#include <Tempest/StorageBuffer>
#include <Tempest/Texture2d>

#include <cstddef>
#include <utility>

#include "material.h"

class StaticMesh;
class AnimMesh;
class SceneGlobals;

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
        uint32_t commandId() const;
        uint32_t bucketId()  const;

      private:
        DrawStorage* owner = nullptr;
        size_t       id    = 0;
      };

    DrawStorage(const SceneGlobals& globals);
    ~DrawStorage();

    Item alloc(const StaticMesh& mesh, const Material& mat,
               size_t iboOff, size_t iboLen, const Tempest::StorageBuffer& desc, Type bucket);

    void prepareUniforms();
    void visibilityPass (Tempest::Encoder<Tempest::CommandBuffer> &cmd, uint8_t fId);
    void drawGBuffer    (Tempest::Encoder<Tempest::CommandBuffer> &cmd, uint8_t fId);
    void drawShadow     (Tempest::Encoder<Tempest::CommandBuffer>& enc, uint8_t fId, int layer);

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
      L_MorphId  = 8,
      L_Morph    = 9,
      L_Pfx      = L_MorphId,
      L_SceneClr = 10,
      L_GDepth   = 11,
      L_HiZ      = 12,
      L_SkyLut   = 13,
      L_Payload  = 14,
      L_Sampler  = 15,
      };

    struct Object {
      bool     isEmpty() const { return cmdId==uint32_t(-1); }

      uint32_t iboOff        = 0;
      uint32_t iboLen        = 0;
      uint32_t bucketId      = 0;
      uint32_t cmdId         = 0;
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

    struct DrawCmd {
      const Tempest::RenderPipeline* pso          = nullptr;
      const Tempest::StorageBuffer*  clusters     = nullptr;
      uint32_t                       bucketId     = 0; // bindfull only

      Tempest::DescriptorSet         desc;
      Material::AlphaFunc            alpha        = Material::Solid;
      uint32_t                       firstCluster = 0;
      uint32_t                       maxClusters  = 0;
      };

    void                           free(size_t id);

    void                           commit();
    const Tempest::RenderPipeline* pipeline (const Material& m);
    uint32_t                       commandId(const Material& m, const Tempest::StorageBuffer* clusters, uint32_t bucketId);

    uint32_t                       bucketId (const Material& m, const StaticMesh& mesh);

    const SceneGlobals&            globals;
    bool                           commited = false;

    std::vector<Object>            objects;
    std::vector<Bucket>            buckets;

    uint32_t                       clusterTotal = 0;
    Tempest::DescriptorSet         descInit, descTask;

    Tempest::StorageBuffer         visClusters, indirectCmd;
    Tempest::DescriptorSet         desc, descClr, descRes;
    std::vector<DrawCmd>           cmd;
    std::vector<DrawCmd*>          cmdOrder;
  };
