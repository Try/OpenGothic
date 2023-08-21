#pragma once

#include <Tempest/Device>
#include <Tempest/Matrix4x4>
#include <Tempest/UniformBuffer>

#include "graphics/meshobjects.h"

class World;
class SceneGlobals;
class LightSource;
class PackedMesh;
class WorldView;

class Landscape final {
  public:
    Landscape(VisualObjects& visual, const PackedMesh& wmesh);

  private:
    using Item = ObjectsBucket::Item;

    struct Block {
      Item                           mesh;
      Tempest::AccelerationStructure blas;
      };

    std::vector<Block>     blocks;
    StaticMesh             mesh;
    Tempest::StorageBuffer meshletDesc;

    Block                  solidBlock;
    Tempest::StorageBuffer meshletSolidDesc;
  };
