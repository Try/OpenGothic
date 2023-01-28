#pragma once

#include <Tempest/VertexBuffer>
#include <Tempest/IndexBuffer>
#include <Tempest/Device>
#include <Tempest/Matrix4x4>

#include <phoenix/model.hh>
#include <phoenix/model_mesh.hh>
#include <phoenix/model_hierarchy.hh>
#include <phoenix/morph_mesh.hh>

#include "graphics/mesh/submesh/staticmesh.h"
#include "graphics/mesh/submesh/animmesh.h"

#include "resources.h"

class PackedMesh;

class ProtoMesh {
  public:
    using Vertex = Resources::VertexA;

    ProtoMesh(PackedMesh&&  pm, std::string_view fname);
    ProtoMesh(PackedMesh&&  pm, const std::vector<phoenix::morph_animation>& aniList, std::string_view fname);
    ProtoMesh(const phoenix::model& lib, std::unique_ptr<Skeleton>&& sk, std::string_view fname);
    ProtoMesh(const phoenix::model_hierarchy& lib, std::unique_ptr<Skeleton>&& sk, std::string_view fname);
    ProtoMesh(const phoenix::model_mesh& lib, std::unique_ptr<Skeleton>&& sk, std::string_view fname);
    ProtoMesh(const Material& mat, std::vector<Resources::Vertex> vbo, std::vector<uint32_t> ibo); //decals
    ProtoMesh(ProtoMesh&&)=delete;
    ProtoMesh& operator=(ProtoMesh&&)=delete;
    ~ProtoMesh();

    struct SubMesh final {
      Material                         material;
      Tempest::IndexBuffer<uint32_t>   ibo;
      std::unique_ptr<PhysicMeshShape> shape;
      };

    struct SubMeshId final {
      size_t                id    = 0;
      size_t                subId = 0;
      };

    struct Node {
      size_t                attachId  = size_t(-1);
      size_t                parentId  = size_t(-1);
      bool                  hasChild  = false;
      Tempest::Matrix4x4    transform;

      size_t                submeshIdB = 0;
      size_t                submeshIdE = 0;
      };

    struct Attach : StaticMesh {
      using StaticMesh::StaticMesh;
      Attach(Attach&&)=default;
      Attach& operator=(Attach&&)=default;
      ~Attach();

      std::string                      name;
      bool                             hasNode=false;
      std::unique_ptr<PhysicMeshShape> shape;
      };

    struct Pos {
      std::string        name;
      Tempest::Matrix4x4 transform;
      size_t             node=0;
      };

    using Morph = StaticMesh::Morph;

    // animation
    std::unique_ptr<Skeleton>      skeleton;
    std::vector<AnimMesh>          skined;
    std::vector<Morph>             morph;
    Tempest::StorageBuffer         morphIndex;
    Tempest::StorageBuffer         morphSamples;

    std::vector<Attach>            attach;
    std::vector<Node>              nodes;
    std::vector<SubMeshId>         submeshId;

    std::vector<Pos>               pos;
    Tempest::Vec3                  bbox[2];

    std::string                    scheme, fname;

    size_t                         skinedNodesCount() const;
    Tempest::Matrix4x4             mapToRoot(size_t node) const;
    size_t                         findNode(std::string_view name,size_t def=size_t(-1)) const;

  private:
    void                           setupScheme(std::string_view s);
    void                           remap(const phoenix::morph_animation& a,
                                         const std::vector<uint32_t>& vertId,
                                         std::vector<int32_t>& remapId,
                                         std::vector<Tempest::Vec4>& samples,
                                         size_t idOffset);

    Morph                          mkAnimation(const phoenix::morph_animation& a);
  };
