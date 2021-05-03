#pragma once

#include "objectsbucket.h"
#include "graphics/sky/sky.h"

class SceneGlobals;
class AnimMesh;

class VisualObjects final {
  public:
    VisualObjects(const SceneGlobals& globals);

    ObjectsBucket::Item get(const StaticMesh& mesh, const Material& mat, size_t iboOffset, size_t iboLen,
                            const ProtoMesh* anim, bool staticDraw);
    ObjectsBucket::Item get(const AnimMesh&   mesh, const Material& mat, const SkeletalStorage::AnimationId& anim, size_t ibo, size_t iboLen);
    ObjectsBucket::Item get(Tempest::VertexBuffer<Resources::Vertex>& vbo, Tempest::IndexBuffer<uint32_t>& ibo,
                            const Material& mat, const Bounds& bbox, ObjectsBucket::Type bucket);
    ObjectsBucket::Item get(const Tempest::VertexBuffer<Resources::Vertex>* vbo[],
                            const Material& mat, const Bounds& bbox);

    SkeletalStorage::AnimationId getAnim(size_t boneCnt);

    void setupUbo();
    void preFrameUpdate(uint8_t fId);
    void visibilityPass(const Frustrum fr[]);
    void draw          (Tempest::Encoder<Tempest::CommandBuffer>& enc, uint8_t fId);
    void drawGBuffer   (Tempest::Encoder<Tempest::CommandBuffer>& enc, uint8_t fId);
    void drawShadow    (Tempest::Encoder<Tempest::CommandBuffer>& enc, uint8_t fId, int layer=0);

    void setWorld   (const World& world);
    void setDayNight(float dayF);
    void resetIndex();

  private:
    ObjectsBucket&                  getBucket(const Material& mat, const ProtoMesh* anim, ObjectsBucket::Type type);
    void                            mkIndex();
    void                            commitUbo(uint8_t fId);

    const SceneGlobals&             globals;
    VisibilityGroup                 visGroup;
    SkeletalStorage                 skinedAnim;

    ObjectsBucket::Storage          uboStatic;
    ObjectsBucket::Storage          uboDyn;

    std::list<ObjectsBucket>        buckets;
    std::vector<ObjectsBucket*>     index;
    size_t                          lastSolidBucket = 0;

    Sky                             sky;

  friend class ObjectsBucket;
  friend class ObjectsBucket::Item;
  };

