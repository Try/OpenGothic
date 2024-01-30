#pragma once

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

    DrawStorage::Item   get(const StaticMesh& mesh, const Material& mat,
                            size_t iboOffset, size_t iboLength, bool staticDraw);
    DrawStorage::Item   get(const AnimMesh& mesh, const Material& mat,
                          size_t iboOff, size_t iboLen,
                          const InstanceStorage::Id& anim);
    DrawStorage::Item   get(const StaticMesh& mesh, const Material& mat,
                            size_t iboOff, size_t iboLen, const PackedMesh::Cluster* cluster,
                            DrawStorage::Type type);

    InstanceStorage::Id alloc(size_t size);
    bool                realloc(InstanceStorage::Id& id, size_t size);
    auto                instanceSsbo() const -> const Tempest::StorageBuffer&;

    void prepareUniforms();
    void preFrameUpdate (uint8_t fId);
    void prepareGlobals (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);
    void postFrameupdate();

    void visibilityPass (Tempest::Encoder<Tempest::CommandBuffer> &cmd, uint8_t frameId, int pass);

    void drawTranslucent(Tempest::Encoder<Tempest::CommandBuffer>& enc, uint8_t fId);
    void drawWater      (Tempest::Encoder<Tempest::CommandBuffer>& enc, uint8_t fId);
    void drawGBuffer    (Tempest::Encoder<Tempest::CommandBuffer>& enc, uint8_t fId);
    void drawShadow     (Tempest::Encoder<Tempest::CommandBuffer>& enc, uint8_t fId, int layer);
    void drawHiZ        (Tempest::Encoder<Tempest::CommandBuffer>& enc, uint8_t fId);

    void notifyTlas(const Material& m, RtScene::Category cat);
    bool updateRtScene(RtScene& out);

    void dbgClusters(Tempest::Painter& p, Tempest::Vec2 wsz);

  private:
    const SceneGlobals& scene;
    InstanceStorage     instanceMem;
    DrawStorage         drawMem;
  };

