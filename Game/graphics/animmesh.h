#pragma once

#include <Tempest/VertexBuffer>
#include <Tempest/IndexBuffer>
#include <Tempest/Device>
#include <Tempest/Matrix4x4>

#include "resources.h"
#include "staticmesh.h"

class AnimMesh {
  public:
    using Vertex =Resources::VertexA;

    AnimMesh(const ZenLoad::zCModelMeshLib& lib);
    AnimMesh(const ZenLoad::PackedMesh&     pm);

    struct SubMesh final {
      Tempest::Texture2d*            texture=nullptr;
      Tempest::IndexBuffer<uint32_t> ibo;
      };

    struct SubMeshId final {
      size_t id      =0;
      size_t subId   =0;
      };

    struct Attach final {
      Attach()=default;
      Attach(Attach&&)=default;
      Attach(const ZenLoad::PackedMesh& m):mesh(m){}
      StaticMesh         mesh;
      };

    struct Node {
      size_t                attachId  = size_t(-1);
      size_t                parentId  = size_t(-1);
      bool                  hasChild  = false;
      Tempest::Matrix4x4    transform;

      size_t                submeshIdB = 0;
      size_t                submeshIdE = 0;
      };

    // skinned
    Tempest::VertexBuffer<Vertex>  vbo;
    std::vector<SubMesh>           sub;

    // offset matrix or static
    std::vector<Attach>            attach;
    std::vector<Node>              nodes;
    std::vector<SubMeshId>         submeshId;

    std::array<float,3>            rootTr;
  };
