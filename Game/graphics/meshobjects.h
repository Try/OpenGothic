#pragma once

#include <Tempest/Texture2d>
#include <Tempest/Matrix4x4>
#include <Tempest/Uniforms>
#include <Tempest/UniformBuffer>

#include <vector>
#include <list>

#include "graphics/submesh/staticmesh.h"
#include "graphics/submesh/animmesh.h"
#include "protomesh.h"
#include "objectsbucket.h"
#include "ubochain.h"
#include "ubostorage.h"

class RendererStorage;
class Pose;
class Light;

class MeshObjects final {
  private:
    using Item = AbstractObjectsBucket::Item;

  public:
    MeshObjects(const RendererStorage& storage);

    class Mesh;
    class Node;

    class Node final {
      public:
        Node(Node&&)=default;

        const Tempest::Texture2d &texture() const;
        void                      draw(Tempest::Encoder<Tempest::CommandBuffer> &cmd, const Tempest::RenderPipeline &pipeline, uint32_t imgId) const;

      private:
        Node(const Item* it):it(it){}
        const Item* it=nullptr;

      friend class Mesh;
      };

    class Mesh final {
      public:
        Mesh()=default;
        Mesh(const ProtoMesh* mesh,std::unique_ptr<Item[]>&& sub,size_t subCount):sub(std::move(sub)),subCount(subCount),ani(mesh){}
        Mesh(Mesh&& other);
        Mesh& operator = (Mesh&& other);

        void   setObjMatrix(const Tempest::Matrix4x4& mt);
        void   setAttachPoint(const Skeleton* sk,const char* defBone=nullptr);
        void   setSkeleton   (const Pose&      p,const Tempest::Matrix4x4& obj);

        auto   attachPoint() const -> const char*;
        bool   isEmpty()    const { return subCount==0; }
        size_t nodesCount() const { return subCount;    }
        Node   node(size_t i) const { return Node(&sub[i]); }

      private:
        std::unique_ptr<Item[]> sub;
        size_t                  subCount=0;
        const ProtoMesh*        ani=nullptr;
        const Skeleton*         skeleton=nullptr;
        const AttachBinder*     binder=nullptr;

        void setObjMatrix(const ProtoMesh &ani, const Tempest::Matrix4x4& mt, size_t parent);
      };

    Mesh get(const StaticMesh& mesh);
    Mesh get(const ProtoMesh&  mesh,int32_t headTexVar,int32_t teethTex,int32_t bodyColor);

    void updateUbo(uint32_t imgId);
    void commitUbo(uint32_t imgId, const Tempest::Texture2d &shadowMap);

    void reserve(size_t stat,size_t dyn);

    void draw      (Tempest::Encoder<Tempest::CommandBuffer> &cmd, uint32_t imgId);
    void drawShadow(Tempest::Encoder<Tempest::CommandBuffer> &cmd, uint32_t imgId, int layer=0);

    bool needToUpdateCommands() const;
    void setAsUpdated();

    void setModelView(const Tempest::Matrix4x4& m, const Tempest::Matrix4x4 *sh, size_t shCount);
    void setLight(const Light &l, const Tempest::Vec3 &ambient);

  private:
    using Vertex  = Resources::Vertex;
    using VertexA = Resources::VertexA;

    struct UboGlobal final {
      std::array<float,3>           lightDir={{0,0,1}};
      float                         padding=0;
      Tempest::Matrix4x4            modelView;
      Tempest::Matrix4x4            shadowView;
      std::array<float,4>           lightAmb={{0,0,0}};
      std::array<float,4>           lightCl ={{1,1,1}};
      };

    struct UboSt final {
      Tempest::Matrix4x4 obj;
      void setObjMatrix(const Tempest::Matrix4x4& ob) { obj=ob; }
      void setSkeleton (const Skeleton*){}
      void setSkeleton (const Pose&    ){}
      };

    struct UboDn final {
      Tempest::Matrix4x4 obj;
      Tempest::Matrix4x4 skel[Resources::MAX_NUM_SKELETAL_NODES];

      void setObjMatrix(const Tempest::Matrix4x4& ob) { obj=ob; }
      void setSkeleton (const Skeleton* sk);
      void setSkeleton (const Pose&      p);
      };

    const RendererStorage&                  storage;

    UboStorage<UboSt>                       storageSt;
    UboStorage<UboDn>                       storageDn;

    std::list<ObjectsBucket<UboSt,Vertex >> chunksSt;
    std::list<ObjectsBucket<UboDn,VertexA>> chunksDn;

    UboChain<UboGlobal,void>        uboGlobalPf[2];
    UboGlobal                       uboGlobal;
    Tempest::Matrix4x4              shadowView1;

    ObjectsBucket<UboSt,Vertex>&    getBucketSt(const Tempest::Texture2d* mat);
    ObjectsBucket<UboSt,Vertex>&    getBucketAt(const Tempest::Texture2d* mat);
    ObjectsBucket<UboDn,VertexA>&   getBucketDn(const Tempest::Texture2d* mat);

    Item                            implGet(const StaticMesh& mesh,
                                            const Tempest::Texture2d* mat,
                                            const Tempest::IndexBuffer<uint32_t> &ibo);
    Item                            implGet(const AnimMesh& mesh,
                                            const Tempest::Texture2d* mat,
                                            const Tempest::IndexBuffer<uint32_t> &ibo);
    const Tempest::Texture2d*       solveTex(const Tempest::Texture2d* def,const std::string& format,int32_t v,int32_t c);
  };
