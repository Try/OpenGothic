#pragma once

#include <Tempest/Device>
#include <Tempest/Matrix4x4>
#include <Tempest/UniformBuffer>

#include "graphics/visualobjects.h"
#include "graphics/mesh/submesh/staticmesh.h"

class PackedMesh;

class Landscape final {
  public:
    Landscape(VisualObjects& visual, const PackedMesh& wmesh);

    const Tempest::StorageBuffer& bvh()   const { return bvhNodes;   }
    const Tempest::StorageBuffer& bvh8()  const { return bvh8Nodes;  }
    // const Tempest::StorageBuffer& bvh64() const { return bvhNodes64; }
    // Tempest::StorageBuffer bvhNodes64, bvh64Ibo, bvh64Vbo;

  private:
    using Item = VisualObjects::Item;

    struct Block {
      Item mesh;
      };

    std::vector<Block>     blocks;
    StaticMesh             mesh;
    Tempest::StorageBuffer meshletDesc;

    Tempest::StorageBuffer bvhNodes;
    Tempest::StorageBuffer bvh8Nodes;
  };
