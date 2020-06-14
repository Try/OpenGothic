#pragma once

#include <Tempest/Texture2d>
#include <Tempest/VertexBuffer>
#include <Tempest/IndexBuffer>
#include <Tempest/UniformBuffer>
#include <Tempest/UniformsLayout>

#include "abstractobjectsbucket.h"
#include "bounds.h"
#include "material.h"
#include "resources.h"
#include "sceneglobals.h"
#include "ubostorage.h"

class RendererStorage;
class Painter3d;

class ObjectsBucket : public AbstractObjectsBucket {
  public:
    using Vertex  = Resources::Vertex;
    using VertexA = Resources::VertexA;

    enum Type : uint8_t {
      Static,
      Movable,
      Animated,
      };

    struct ShLight final {
      Tempest::Vec3 pos;
      float         padding=0;
      Tempest::Vec3 color;
      float         range=0;
      };

    struct UboObject final {
      Tempest::Matrix4x4 pos;
      ShLight            light[6];
      };

    struct UboAnim final {
      Tempest::Matrix4x4 skel[Resources::MAX_NUM_SKELETAL_NODES];
      };

    struct Storage final {
      UboStorage<UboObject> st;
      UboStorage<UboAnim>   sk;
      bool                  commitUbo(Tempest::Device &device, uint8_t fId);
      };

    ObjectsBucket(const Material& mat, const SceneGlobals& scene, Storage& storage, const Type type);
    ~ObjectsBucket();

    const Material&           material() const;
    Type                      type()     const { return shaderType; }

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
    void                      draw      (size_t id, Painter3d& p, uint8_t fId);

  private:
    struct Object final {
      const Tempest::VertexBuffer<Vertex>*  vbo  = nullptr;
      const Tempest::VertexBuffer<VertexA>* vboA = nullptr;
      const Tempest::IndexBuffer<uint32_t>* ibo  = nullptr;
      Bounds                                bounds;
      size_t                                storageSt = size_t(-1);
      size_t                                storageSk = size_t(-1);
      Tempest::Uniforms                     ubo  [Resources::MaxFramesInFlight];
      Tempest::Uniforms                     uboSh[Resources::MaxFramesInFlight][Resources::ShadowLayers];
      bool                                  visible = false;
      };

    std::vector<Object>       val;
    std::vector<size_t>       freeList;

    const SceneGlobals&       scene;
    Storage&                  storage;
    Material                  mat;
    const Type                shaderType;

    const Tempest::RenderPipeline* pMain   = nullptr;
    const Tempest::RenderPipeline* pShadow = nullptr;

    Object& implAlloc(const Tempest::IndexBuffer<uint32_t> &ibo, const Bounds& bounds);
    void    uboSetCommon(Object& v);

    void   setObjMatrix(size_t i,const Tempest::Matrix4x4& m);
    void   setSkeleton (size_t i,const Skeleton* sk);
    void   setSkeleton (size_t i,const Pose& sk);
    void   setBounds   (size_t i,const Bounds& b);

    void   setupLights (Object& val, UboObject& ubo);
  };

