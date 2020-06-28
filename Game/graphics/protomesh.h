#pragma once

#include <Tempest/VertexBuffer>
#include <Tempest/IndexBuffer>
#include <Tempest/Device>
#include <Tempest/Matrix4x4>

#include "graphics/submesh/staticmesh.h"
#include "graphics/submesh/animmesh.h"

#include "resources.h"

class ProtoMesh {
  public:
    using Vertex =Resources::VertexA;

    ProtoMesh(const ZenLoad::zCModelMeshLib& lib,const std::string& fname);
    ProtoMesh(ZenLoad::PackedMesh&&     pm, const std::string& fname);
    ProtoMesh(const ZenLoad::zCVobData& vob, std::vector<Resources::Vertex> vbo, std::vector<uint32_t> ibo);
    ProtoMesh(ProtoMesh&&)=default;
    ProtoMesh& operator=(ProtoMesh&&)=default;
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

    // skinned
    std::vector<AnimMesh>          skined;

    std::vector<Attach>            attach;
    std::vector<Node>              nodes;
    std::vector<SubMeshId>         submeshId;

    std::vector<Pos>               pos;
    std::array<float,3>            rootTr={};

    Tempest::Vec3                  bbox[2];

    std::string                    scheme;

    size_t                         skinedNodesCount() const;
    Tempest::Matrix4x4             mapToRoot(size_t node) const;

  private:
    void                           setupScheme(const std::string& s);
  };
