#pragma once

#include <Tempest/Texture2d>
#include <Tempest/Matrix4x4>
#include <Tempest/DescriptorSet>
#include <Tempest/UniformBuffer>

#include <vector>
#include <list>

#include "graphics/mesh/protomesh.h"
#include "graphics/mesh/submesh/staticmesh.h"
#include "graphics/mesh/submesh/animmesh.h"
#include "objectsbucket.h"

class VisualObjects;
class Pose;
class ProtoMesh;
class LightSource;

class MeshObjects final {
  private:
    using Item = ObjectsBucket::Item;

  public:
    MeshObjects(VisualObjects& parent);
    ~MeshObjects();

    class Mesh;
    class Node;

    class Node final {
      public:
        Node(Node&&)=default;

        void draw(Tempest::Encoder<Tempest::CommandBuffer>& p, uint8_t fId) const;
        const Bounds& bounds() const;

      private:
        Node(const Item* it):it(it){}
        const Item* it=nullptr;

      friend class Mesh;
      };

    class Mesh final {
      public:
        Mesh()=default;
        Mesh(MeshObjects& owner, const StaticMesh& mesh, int32_t version, bool staticDraw);
        Mesh(MeshObjects& owner, const StaticMesh& mesh, int32_t headTexVar, int32_t teethTex, int32_t bodyColor);
        Mesh(MeshObjects& owner, const ProtoMesh&  mesh, int32_t headTexVar, int32_t teethTex, int32_t bodyColor, bool staticDraw);
        Mesh(Mesh&& other);
        ~Mesh();
        Mesh& operator = (Mesh&& other);

        void   setObjMatrix(const Tempest::Matrix4x4& mt);
        void   setSkeleton (const Skeleton* sk);
        void   setPose     (const Tempest::Matrix4x4& obj, const Pose& p);
        void   setAsGhost  (bool g);
        void   setFatness  (float f);
        void   setWind     (ZenLoad::AnimMode m);
        void   startMMAnim (std::string_view anim, float intensity, uint64_t timeUntil);

        bool   isEmpty()    const { return subCount==0; }
        size_t nodesCount() const { return subCount;    }
        Node   node(size_t i) const { return Node(&sub[i]); }

        Bounds bounds() const;
        const ProtoMesh* protoMesh() const { return proto; }

        const PfxEmitterMesh* toMeshEmitter() const;

      private:
        void implSetObjMatrix(const Tempest::Matrix4x4& mt, const Tempest::Matrix4x4* tr);
        void implSetObjMatrix(const ProtoMesh &ani, const Tempest::Matrix4x4& mt, size_t parent);

        std::unique_ptr<Item[]> sub;
        size_t                  subCount=0;
        std::unique_ptr<SkeletalStorage::AnimationId> anim;

        const ProtoMesh*        proto   =nullptr;
        const Skeleton*         skeleton=nullptr;
        const AttachBinder*     binder  =nullptr;
      };

  private:
    VisualObjects&                  parent;

    Item                            implGet(const StaticMesh& mesh, const StaticMesh::SubMesh& smesh,
                                            const ProtoMesh* anim,
                                            int32_t texVar, int32_t teethTex, int32_t bodyColor,
                                            bool staticDraw);

    const Tempest::Texture2d*       solveTex(const Tempest::Texture2d* def,const std::string& format,
                                             int32_t v,int32_t c);
  };
