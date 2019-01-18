#pragma once

#include <Tempest/Texture2d>
#include <Tempest/Matrix4x4>
#include <Tempest/Uniforms>
#include <Tempest/UniformBuffer>
#include <Tempest/AlignedArray>

#include <vector>
#include <list>

#include "staticmesh.h"

class StaticObjects final {
  struct Chunk;
  public:
    StaticObjects(Tempest::Device &device);

    const Tempest::UniformsLayout& uboLayout(){ return layout; }

    class Obj {
      public:
        Obj()=default;
        Obj(Chunk& owner,size_t id,const StaticMesh* mesh,const Tempest::IndexBuffer<uint32_t>* mibo)
          :owner(&owner),id(id),mesh(mesh),mibo(mibo){}

        size_t orderId() const { return uintptr_t(owner); }
        void   setMatrix(const Tempest::Matrix4x4& mt);

        const Tempest::VertexBuffer<Resources::Vertex>& vbo() const { return mesh->vbo; }
        const Tempest::IndexBuffer <uint32_t>&          ibo() const { return *mibo; }

      private:
        Chunk*                                owner=nullptr;
        size_t                                id=0;
        const StaticMesh*                     mesh=nullptr;
        const Tempest::IndexBuffer<uint32_t>* mibo=nullptr;

      friend class StaticObjects;
      };

    Obj  get(const Tempest::Texture2d* mat, const StaticMesh& mesh, const Tempest::IndexBuffer<uint32_t> &ibo);

    void setMatrix(uint32_t imgId);
    void commitUbo(uint32_t imgId);
    void setUniforms(Tempest::CommandBuffer &cmd,Tempest::RenderPipeline& pipe,uint32_t imgId,const StaticObjects::Obj &obj);

  private:
    struct Ubo {
      Tempest::Matrix4x4 mat;
      };

    struct PerFrame {
      Tempest::Uniforms      ubo;
      Tempest::UniformBuffer uboData;
      };

    struct Chunk {
      Chunk(const Tempest::Texture2d* tex,size_t uboAlign):tex(tex),obj(uboAlign){}

      const Tempest::Texture2d*   tex;
      std::unique_ptr<PerFrame[]> pf;
      Tempest::AlignedArray<Ubo>  obj;
      };

    Tempest::Device&        device;
    std::list<Chunk>        chunks;
    Tempest::UniformsLayout layout;

    Obj get(Chunk& owner, const StaticMesh &mesh, const Tempest::IndexBuffer<uint32_t>& ibo);
  };
