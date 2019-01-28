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

    ProtoMesh(const ZenLoad::zCModelMeshLib& lib);
    ProtoMesh(const ZenLoad::PackedMesh&     pm);

    struct SubMesh final {
      Tempest::Texture2d*            texture=nullptr;
      Tempest::IndexBuffer<uint32_t> ibo;
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

    // skinned
    std::vector<AnimMesh>          skined;

    // offset matrix or static
    std::vector<StaticMesh>        attach;
    std::vector<Node>              nodes;
    std::vector<SubMeshId>         submeshId;

    std::array<float,3>            rootTr;

    size_t                         skinedNodesCount() const;
  };
