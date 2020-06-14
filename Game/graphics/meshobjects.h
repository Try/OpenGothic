#pragma once

#include <Tempest/Texture2d>
#include <Tempest/Matrix4x4>
#include <Tempest/Uniforms>
#include <Tempest/UniformBuffer>

#include <vector>
#include <list>

#include "graphics/submesh/staticmesh.h"
#include "graphics/submesh/animmesh.h"
#include "abstractobjectsbucket.h"
#include "objectsbucket.h"
#include "protomesh.h"
#include "ubochain.h"
#include "ubostorage.h"

class SceneGlobals;
class Pose;
class Light;
class Painter3d;
class ObjectsBucket;

class MeshObjects final {
  private:
    using Item = AbstractObjectsBucket::Item;

  public:
    MeshObjects(const SceneGlobals& globals);
    ~MeshObjects();

    class Mesh;
    class Node;

    class Node final {
      public:
        Node(Node&&)=default;

        void draw(Painter3d &p, uint8_t fId) const;

      private:
        Node(const Item* it):it(it){}
        const Item* it=nullptr;

      friend class Mesh;
      };

    class Mesh final {
      public:
        Mesh()=default;
        Mesh(const ProtoMesh* mesh,
             std::unique_ptr<Item[]>&& sub,size_t subCount)
          :sub(std::move(sub)),subCount(subCount),ani(mesh){}
        Mesh(Mesh&& other);
        Mesh& operator = (Mesh&& other);

        void   setObjMatrix(const Tempest::Matrix4x4& mt);
        void   setSkeleton (const Skeleton* sk);
        void   setPose     (const Pose&      p,const Tempest::Matrix4x4& obj);

        bool   isEmpty()    const { return subCount==0; }
        size_t nodesCount() const { return subCount;    }
        Node   node(size_t i) const { return Node(&sub[i]); }

        Tempest::Vec3    translate() const;
        const ProtoMesh* protoMesh() const { return ani; }

      private:
        std::unique_ptr<Item[]> sub;
        size_t                  subCount=0;

        const ProtoMesh*        ani=nullptr;
        const Skeleton*         skeleton=nullptr;
        const AttachBinder*     binder=nullptr;

        void setObjMatrix(const ProtoMesh &ani, const Tempest::Matrix4x4& mt, size_t parent);
      };

    Mesh get(const StaticMesh& mesh, bool staticDraw);
    Mesh get(const StaticMesh& mesh, int32_t headTexVar, int32_t teethTex, int32_t bodyColor);
    Mesh get(const ProtoMesh&  mesh, int32_t headTexVar, int32_t teethTex, int32_t bodyColor, bool staticDraw);
    Mesh get(Tempest::VertexBuffer<Resources::Vertex>& vbo,
             Tempest::IndexBuffer<uint32_t>& ibo,
             const Material& mat, const Bounds& bbox);

    void setupUbo();

    void draw      (Painter3d& painter, uint8_t fId);
    void drawShadow(Painter3d& painter, uint8_t fId, int layer=0);

  private:
    const SceneGlobals&             globals;

    ObjectsBucket::Storage          uboStatic;
    ObjectsBucket::Storage          uboDyn;

    std::list<ObjectsBucket>        buckets;
    std::vector<ObjectsBucket*>     index;

    void                            mkIndex();

    ObjectsBucket&                  getBucket(const Material& mat, ObjectsBucket::Type type);

    Item                            implGet(const StaticMesh& mesh, const StaticMesh::SubMesh& smesh,
                                            int32_t texVar, int32_t teethTex, int32_t bodyColor,
                                            bool staticDraw);
    Item                            implGet(const StaticMesh& mesh,
                                            const Material& mat,
                                            const Tempest::IndexBuffer<uint32_t> &ibo,
                                            bool staticDraw);
    Item                            implGet(const AnimMesh& mesh,
                                            const Material& mat,
                                            const Tempest::IndexBuffer<uint32_t> &ibo);

    const Tempest::Texture2d*       solveTex(const Tempest::Texture2d* def,const std::string& format,
                                             int32_t v,int32_t c);
  };
