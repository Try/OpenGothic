#pragma once

#include <Tempest/Signal>

#include "objectsbucket.h"
#include "drawstorage.h"

class SceneGlobals;
class RtScene;
class Landscape;
class Sky;
class AnimMesh;

class VisualObjects final {
  public:
    VisualObjects(const SceneGlobals& globals, const std::pair<Tempest::Vec3, Tempest::Vec3>& bbox);
    ~VisualObjects();

    ObjectsBucket::Item get(const StaticMesh& mesh, const Material& mat,
                            size_t iboOffset, size_t iboLength, bool staticDraw);
    ObjectsBucket::Item get(const StaticMesh& mesh, const Material& mat,
                            size_t iboOff, size_t iboLen,
                            const Tempest::StorageBuffer& desc,
                            const Bounds& bbox, ObjectsBucket::Type bucket);
    ObjectsBucket::Item get(const AnimMesh& mesh, const Material& mat,
                            size_t iboOff, size_t iboLen,
                            const InstanceStorage::Id& anim);
    ObjectsBucket::Item get(const Material& mat);

    DrawStorage::Item   getDr(const StaticMesh& mesh, const Material& mat,
                              size_t iboOffset, size_t iboLength, bool staticDraw);
    DrawStorage::Item   getDr(const AnimMesh& mesh, const Material& mat,
                              size_t iboOff, size_t iboLen,
                              const InstanceStorage::Id& anim);
    DrawStorage::Item   getDr(const StaticMesh& mesh, const Material& mat,
                              size_t iboOff, size_t iboLen, const PackedMesh::Cluster* cluster,
                              DrawStorage::Type type);

    InstanceStorage::Id alloc(size_t size);
    bool                realloc(InstanceStorage::Id& id, size_t size);
    auto                instanceSsbo() const -> const Tempest::StorageBuffer&;

    void prepareUniforms();
    void preFrameUpdate (uint8_t fId);
    void prepareGlobals (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);
    void postFrameupdate();

    void visibilityPass (const Frustrum fr[]);
    void visibilityPass (Tempest::Encoder<Tempest::CommandBuffer> &cmd, uint8_t frameId);

    void drawTranslucent(Tempest::Encoder<Tempest::CommandBuffer>& enc, uint8_t fId);
    void drawWater      (Tempest::Encoder<Tempest::CommandBuffer>& enc, uint8_t fId);
    void drawGBuffer    (Tempest::Encoder<Tempest::CommandBuffer>& enc, uint8_t fId);
    void drawShadow     (Tempest::Encoder<Tempest::CommandBuffer>& enc, uint8_t fId, int layer);
    void drawHiZ        (Tempest::Encoder<Tempest::CommandBuffer>& enc, uint8_t fId);

    void resetIndex();
    void notifyTlas(const Material& m, RtScene::Category cat);

    bool updateRtScene(RtScene& out);

    void dbgClusters(Tempest::Painter& p, Tempest::Vec2 wsz);

  private:
    ObjectsBucket& getBucket(ObjectsBucket::Type type, const Material& mat,
                             const StaticMesh* st, const AnimMesh* anim, const Tempest::StorageBuffer* desc);
    void           mkIndex();

    const SceneGlobals&                         globals;
    VisibilityGroup                             visGroup;
    InstanceStorage                             instanceMem;
    DrawStorage                                 drawMem;

    std::vector<std::unique_ptr<ObjectsBucket>> buckets;
    std::vector<ObjectsBucket*>                 index;
    size_t                                      lastSolidBucket = 0;

  friend class ObjectsBucket;
  friend class ObjectsBucket::Item;
  };

