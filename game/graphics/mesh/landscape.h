#pragma once

#include <Tempest/Device>
#include <Tempest/Matrix4x4>
#include <Tempest/UniformBuffer>

#include "graphics/visualobjects.h"
#include "graphics/meshobjects.h"

class PackedMesh;

class Landscape final {
  public:
    Landscape(VisualObjects& visual, const PackedMesh& wmesh);

  private:
    using Item = VisualObjects::Item;

    struct Block {
      Item mesh;
      };

    std::vector<Block>     blocks;
    StaticMesh             mesh;
    Tempest::StorageBuffer meshletDesc;
  };
