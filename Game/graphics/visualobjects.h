#pragma once

#include "objectsbucket.h"
#include "graphics/sky/sky.h"

class SceneGlobals;
class AnimMesh;

class VisualObjects final {
  public:
    VisualObjects(const SceneGlobals& globals);

    ObjectsBucket::Item get(const StaticMesh& mesh, const Material& mat, const Tempest::IndexBuffer<uint32_t>& ibo, bool staticDraw);
    ObjectsBucket::Item get(const AnimMesh&   mesh, const Material& mat, const Tempest::IndexBuffer<uint32_t>& ibo);
    ObjectsBucket::Item get(Tempest::VertexBuffer<Resources::Vertex>& vbo, Tempest::IndexBuffer<uint32_t>& ibo,
                            const Material& mat, const Bounds& bbox);
    ObjectsBucket::Item get(const Tempest::VertexBuffer<Resources::Vertex>* vbo[],
                            const Material& mat, const Bounds& bbox);

    void setupUbo();
    void preFrameUpdate(uint8_t fId);
    void draw          (Painter3d& painter, Tempest::Encoder<Tempest::CommandBuffer>& enc, uint8_t fId);
    void drawShadow    (Painter3d& painter, Tempest::Encoder<Tempest::CommandBuffer>& enc, uint8_t fId, int layer=0);

    void setWorld   (const World& world);
    void setDayNight(float dayF);

  private:
    ObjectsBucket&                  getBucket(const Material& mat, ObjectsBucket::Type type);
    void                            mkIndex();
    void                            commitUbo(uint8_t fId);

    const SceneGlobals&             globals;

    ObjectsBucket::Storage          uboStatic;
    ObjectsBucket::Storage          uboDyn;

    std::list<ObjectsBucket>        buckets;
    std::vector<ObjectsBucket*>     index;
    size_t                          lastSolidBucket = 0;

    Sky                             sky;
  };

