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

    using UboGlobal = MeshObjects::UboGlobal;

    ObjectsBucket(const Tempest::Texture2d* tex, const RendererStorage& storage, const Type type);
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

    void                      draw      (Painter3d& painter, uint8_t fId, const Tempest::UniformBuffer<UboGlobal>& uboGlobal, const Tempest::Texture2d& shadowMap);
    void                      drawShadow(Painter3d& painter, uint8_t fId, const Tempest::UniformBuffer<UboGlobal>& uboGlobal, int layer=0);
    void                      draw      (size_t id, Tempest::Encoder<Tempest::CommandBuffer> &cmd,
                                         const Tempest::RenderPipeline &pipeline, uint32_t fId);

  private:
    struct Object {
      const Tempest::VertexBuffer<Vertex>*  vbo  = nullptr;
      const Tempest::VertexBuffer<VertexA>* vboA = nullptr;
      const Tempest::IndexBuffer<uint32_t>* ibo  = nullptr;
      Bounds                                bounds;
      size_t                                storageSt = 0;
      size_t                                storageSk = 0;
      Tempest::Uniforms                     ubo  [Resources::MaxFramesInFlight];
      Tempest::Uniforms                     uboSh[Resources::MaxFramesInFlight];
      };

    struct SkMatrix final {
      Tempest::Matrix4x4 skel[Resources::MAX_NUM_SKELETAL_NODES];
      };

    std::vector<Object>       val;
    std::vector<size_t>       freeList;

    UboStorage<Tempest::Matrix4x4> storageSt;
    UboStorage<SkMatrix>           storageSk;

    const RendererStorage&    storage;
    const Tempest::Texture2d* tex = nullptr;
    const Type                shaderType;

    Object& implAlloc(const Tempest::IndexBuffer<uint32_t> &ibo, const Bounds& bounds, const Tempest::UniformsLayout& layout, const Tempest::UniformsLayout& layoutSh);
    void    uboSetCommon(Object& v);

    void   setObjMatrix(size_t i,const Tempest::Matrix4x4& m);
    void   setSkeleton (size_t i,const Skeleton* sk);
    void   setSkeleton (size_t i,const Pose& sk);
    void   setBounds   (size_t i,const Bounds& b);
  };

