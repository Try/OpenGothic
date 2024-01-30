#pragma once

#include <Tempest/Device>
#include <Tempest/Matrix4x4>
#include <Tempest/UniformBuffer>

#include "graphics/drawstorage.h"
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
    using Item = DrawStorage::Item;

    struct Block {
      Item mesh;
      };

    std::vector<Block>     blocks;
    StaticMesh             mesh;
    Tempest::StorageBuffer meshletDesc;
  };
