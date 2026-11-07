#pragma once

#include <Tempest/Device>
#include <Tempest/Matrix4x4>
#include <Tempest/UniformBuffer>

#include "graphics/visualobjects.h"
#include "graphics/mesh/submesh/packedmesh.h"
#include "graphics/mesh/submesh/staticmesh.h"

class Landscape final {
  public:
    // Immutable GPU data of the level mesh, which can be moved into a reload of the same level
    struct Mesh final {
      explicit Mesh(const PackedMesh& wmesh);

      StaticMesh                       mesh;
      std::vector<PackedMesh::Cluster> meshletBounds;
      Tempest::StorageBuffer           meshletDesc;
      Tempest::StorageBuffer           bvhNodes;
      };

    Landscape(VisualObjects& visual, std::unique_ptr<Mesh> mesh);

    auto takeMesh() -> std::unique_ptr<Mesh>;

    const Tempest::StorageBuffer& bvh()   const { return data->bvhNodes; }

  private:
    using Item = VisualObjects::Item;

    struct Block {
      Item mesh;
      };

    std::vector<Block>     blocks;
    std::unique_ptr<Mesh>  data;
  };
