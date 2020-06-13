#pragma once

#include <Tempest/Texture2d>
#include <Tempest/VertexBuffer>
#include <Tempest/IndexBuffer>
#include <Tempest/UniformBuffer>
#include <Tempest/UniformsLayout>

#include "abstractobjectsbucket.h"
#include "bounds.h"
#include "meshobjects.h"
#include "resources.h"
#include "ubostorage.h"

class RendererStorage;
class Painter3d;

class ObjectsBucket : public AbstractObjectsBucket {
  public:
    using Vertex  = Resources::Vertex;
    using VertexA = Resources::VertexA;

    enum Type : uint8_t {
      Static,
      Animated,
      };

    ObjectsBucket(const Tempest::Texture2d* tex, const SceneGlobals& scene, const Type type);
    ~ObjectsBucket();

    const Tempest::Texture2d& texture() const;
    Type                      type()    const { return shaderType; }

    size_t                    alloc(const Tempest::VertexBuffer<Vertex> &vbo,
                                    const Tempest::IndexBuffer<uint32_t> &ibo,
                                    const Bounds& bounds);
    size_t                    alloc(const Tempest::VertexBuffer<VertexA> &vbo,
                                    const Tempest::IndexBuffer<uint32_t> &ibo,
                                    const Bounds& bounds);
    void                      free(const size_t objId);

    void                      setupUbo();
    void                      draw      (Painter3d& painter, uint8_t fId);
    void                      drawShadow(Painter3d& painter, uint8_t fId, int layer=0);
    void                      draw      (size_t id, Painter3d& p, uint32_t fId);

  private:
    struct Object {
      const Tempest::VertexBuffer<Vertex>*  vbo  = nullptr;
      const Tempest::VertexBuffer<VertexA>* vboA = nullptr;
      const Tempest::IndexBuffer<uint32_t>* ibo  = nullptr;
      Bounds                                bounds;
      size_t                                storageSt = size_t(-1);
      size_t                                storageSk = size_t(-1);
      Tempest::Uniforms                     ubo  [Resources::MaxFramesInFlight];
      Tempest::Uniforms                     uboSh[Resources::MaxFramesInFlight][Resources::ShadowLayers];
      };

    struct ShLight {
      Tempest::Vec3 pos;
      float         padding=0;
      Tempest::Vec3 color;
      float         range=0;
      };

    struct UboObject final {
      Tempest::Matrix4x4 pos;
      ShLight            light[1];
      };

    struct UboAnim final {
      Tempest::Matrix4x4 skel[Resources::MAX_NUM_SKELETAL_NODES];
      };

    std::vector<Object>       val;
    std::vector<size_t>       freeList;

    UboStorage<UboObject>     storageSt;
    UboStorage<UboAnim>       storageSk;

    const SceneGlobals&       scene;
    const Tempest::Texture2d* tex = nullptr;
    const Type                shaderType;

    Object& implAlloc(const Tempest::IndexBuffer<uint32_t> &ibo, const Bounds& bounds, const Tempest::UniformsLayout& layout, const Tempest::UniformsLayout& layoutSh);
    void    uboSetCommon(Object& v);

    void   setObjMatrix(size_t i,const Tempest::Matrix4x4& m);
    void   setSkeleton (size_t i,const Skeleton* sk);
    void   setSkeleton (size_t i,const Pose& sk);
    void   setBounds   (size_t i,const Bounds& b);

    void   setupLights(UboObject& ubo, const Tempest::Vec3& pos);
  };

