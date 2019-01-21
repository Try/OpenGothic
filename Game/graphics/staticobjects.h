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

    class Obj final {
      public:
        Obj()=default;
        Obj(Chunk& owner,size_t id,const StaticMesh* mesh)
          :owner(&owner),id(id),mesh(mesh){}
        Obj(Obj&& obj):owner(obj.owner),id(obj.id),mesh(obj.mesh){
          obj.owner=nullptr;
          }
        Obj& operator=(Obj&& obj) {
          std::swap(obj.owner,owner);
          std::swap(obj.id,   id);
          std::swap(obj.mesh, mesh);
          return *this;
          }
        ~Obj();

        size_t orderId() const { return uintptr_t(owner); }

        void   setObjMatrix(const Tempest::Matrix4x4& mt);

        const Tempest::Matrix4x4& objMatrix();

      private:
        Chunk*                                owner=nullptr;
        size_t                                id=0;

        const StaticMesh*                     mesh=nullptr;
      friend class StaticObjects;
      };

    class Mesh final {
      public:
        Mesh()=default;
        Mesh(std::unique_ptr<Obj[]>&& sub):sub(std::move(sub)){}

      private:
        std::unique_ptr<Obj[]> sub;
      };
    Obj  get(const StaticMesh& mesh, const Tempest::Texture2d* mat, const Tempest::IndexBuffer<uint32_t> &ibo);
    Mesh get(const StaticMesh& mesh);

    void setMatrix(uint32_t imgId);
    void commitUbo(uint32_t imgId);
    void draw     (Tempest::CommandBuffer &cmd,Tempest::RenderPipeline& pipe,uint32_t imgId,const StaticObjects::Obj &obj);
    void draw     (Tempest::CommandBuffer &cmd,Tempest::RenderPipeline& pipe,uint32_t imgId);

    bool needToUpdateCommands() const;
    void setAsUpdated();

    void setModelView(const Tempest::Matrix4x4& m);

  private:
    struct UboGlobal {
      std::array<float,3> lightDir={{0,0,1}};
      float               padding=0;
      Tempest::Matrix4x4  modelView;
      };

    struct Ubo {
      Tempest::Matrix4x4 obj;
      };

    struct NonUbo {
      const Tempest::VertexBuffer<Resources::Vertex>* vbo=nullptr;
      const Tempest::IndexBuffer<uint32_t>*           ibo=nullptr;
      };

    struct PerFrame {
      Tempest::UniformBuffer uboData;
      Tempest::Uniforms      ubo;
      bool                   uboChanged=false; // invalidate ubo array
      };

    struct Chunk {
      Chunk(const Tempest::Texture2d* tex,size_t uboAlign,uint32_t pfSize):tex(tex),pfSize(pfSize),obj(uboAlign){}

      const Tempest::Texture2d*   tex;
      std::unique_ptr<PerFrame[]> pf;
      uint32_t                    pfSize=0;

      Tempest::AlignedArray<Ubo>  obj;
      std::vector<NonUbo>         data;

      std::vector<size_t>         freeList;
      void                        free(const Obj& obj);
      };

    Tempest::Device&            device;
    std::list<Chunk>            chunks;
    Tempest::UniformsLayout     layout;

    std::unique_ptr<PerFrame[]> pf;
    UboGlobal                   uboGlobal;
    bool                        nToUpdate=true; //invalidate cmd buffers

    Obj  implGet(Chunk& owner, const StaticMesh &mesh, const Tempest::IndexBuffer<uint32_t>& ibo);
  };
