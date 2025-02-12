#pragma once

#include "drawbuckets.h"
#include "drawclusters.h"
#include "drawcommands.h"
#include "instancestorage.h"

#include <unordered_set>

class SceneGlobals;
class Camera;
class RtScene;
class Landscape;
class Sky;
class AnimMesh;

class VisualObjects final {
  public:
    class Item final {
      public:
      Item()=default;
      Item(VisualObjects& owner,size_t id)
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
      void     setWind     (zenkit::AnimationType m, float intensity);
      void     startMMAnim (std::string_view anim, float intensity, uint64_t timeUntil);

      const Material&    material() const;
      const Bounds&      bounds()   const;
      Tempest::Matrix4x4 position() const;
      const StaticMesh*  mesh()     const;
      std::pair<uint32_t,uint32_t> meshSlice() const;

      private:
      VisualObjects* owner = nullptr;
      size_t         id    = 0;
      };

    VisualObjects(const SceneGlobals& globals, const std::pair<Tempest::Vec3, Tempest::Vec3>& bbox);
    ~VisualObjects();

    Item   get(const StaticMesh& mesh, const Material& mat, size_t iboOffset, size_t iboLength, bool staticDraw);
    Item   get(const AnimMesh& mesh,   const Material& mat, size_t iboOff, size_t iboLen, const InstanceStorage::Id& anim);
    Item   get(const StaticMesh& mesh, const Material& mat, size_t iboOff, size_t iboLen, const PackedMesh::Cluster* cluster, DrawCommands::Type type);

    InstanceStorage::Id alloc(size_t size);
    bool                realloc(InstanceStorage::Id& id, size_t size);
    auto                instanceSsbo() const -> const Tempest::StorageBuffer&;

    void resetRendering();

    void preFrameUpdate ();
    void prepareGlobals (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);
    void postFrameupdate();

    void visibilityPass (Tempest::Encoder<Tempest::CommandBuffer>& cmd, int pass);
    void visibilityVsm  (Tempest::Encoder<Tempest::CommandBuffer>& cmd);

    void drawTranslucent(Tempest::Encoder<Tempest::CommandBuffer>& cmd);
    void drawWater      (Tempest::Encoder<Tempest::CommandBuffer>& cmd);
    void drawGBuffer    (Tempest::Encoder<Tempest::CommandBuffer>& cmd);
    void drawShadow     (Tempest::Encoder<Tempest::CommandBuffer>& cmd, int layer);
    void drawVsm        (Tempest::Encoder<Tempest::CommandBuffer>& cmd);
    void drawSwr        (Tempest::Encoder<Tempest::CommandBuffer>& cmd);
    void drawHiZ        (Tempest::Encoder<Tempest::CommandBuffer>& cmd);

    bool updateRtScene(RtScene& out);

    void dbgClusters(Tempest::Painter& p, Tempest::Vec2 wsz);

  private:
    struct InstanceDesc;
    struct MorphDesc;
    struct MorphData;

    struct MorphAnim {
      size_t   id        = 0;
      uint64_t timeStart = 0;
      uint64_t timeUntil = 0;
      float    intensity = 0;
      };

    struct Object {
      bool     isEmpty() const { return cmdId==uint16_t(-1); }

      Tempest::Matrix4x4  pos;
      InstanceStorage::Id objInstance;
      InstanceStorage::Id objMorphAnim;

      DrawCommands::Type  type      = DrawCommands::Type::Landscape;
      uint32_t            iboOff    = 0;
      uint32_t            iboLen    = 0;
      uint32_t            animPtr   = 0;
      DrawBuckets::Id     bucketId;
      uint16_t            cmdId     = uint16_t(-1);
      uint32_t            clusterId = 0;
      uint64_t            timeShift = 0;

      Material::AlphaFunc alpha = Material::Solid;
      MorphAnim           morphAnim[Resources::MAX_MORPH_LAYERS];
      zenkit::AnimationType wind        = zenkit::AnimationType::NONE;
      float               windIntensity = 0;
      float               fatness       = 0;
      bool                isGhost       = false;
      };

    void     preFrameUpdateWind();
    void     preFrameUpdateMorph();

    size_t   implAlloc();
    void     free(size_t id);

    uint32_t clusterId(const PackedMesh::Cluster* cx, size_t firstMeshlet, size_t meshletCount, uint16_t bucketId, uint16_t commandId);
    uint32_t clusterId(const DrawBuckets::Bucket& bucket, size_t firstMeshlet, size_t meshletCount, uint16_t bucketId, uint16_t commandId);

    void     startMMAnim(size_t i, std::string_view animName, float intensity, uint64_t timeUntil);
    void     setAsGhost(size_t i, bool g);

    void     notifyTlas(const Material& m, RtScene::Category cat);
    void     updateInstance(size_t id, Tempest::Matrix4x4* pos = nullptr);
    void     updateRtAs(size_t id);

    void     dbgDraw(Tempest::Painter& p, Tempest::Vec2 wsz, const Camera& cam, const DrawClusters::Cluster& cx);
    void     dbgDrawBBox(Tempest::Painter& p, Tempest::Vec2 wsz, const Camera& cam, const DrawClusters::Cluster& c);

    const SceneGlobals&        scene;

    InstanceStorage            instanceMem;
    DrawBuckets                bucketsMem;
    DrawClusters               clusters;
    DrawCommands               drawCmd;

    std::vector<Object>        objects;
    std::unordered_set<size_t> objectsWind;
    std::unordered_set<size_t> objectsMorph;
    std::unordered_set<size_t> objectsFree;

    friend class Item;
  };

