#pragma once

#include <Tempest/Device>
#include <Tempest/Matrix4x4>
#include <Tempest/UniformBuffer>

#include <zenload/zTypes.h>

#include "graphics/bounds.h"
#include "graphics/material.h"
#include "graphics/meshobjects.h"
#include "resources.h"

class World;
class SceneGlobals;
class LightSource;
class PackedMesh;
class WorldView;

class Landscape final {
  public:
    Landscape(VisualObjects& visual, const PackedMesh& wmesh);

    Tempest::AccelerationStructure blas;

  private:
    using Item = ObjectsBucket::Item;

    struct Block {
      Tempest::IndexBuffer<uint32_t> ibo;
      Item                           mesh;
      Tempest::AccelerationStructure blas;
      };

    Tempest::VertexBuffer<Resources::Vertex> vbo;
    // Tempest::IndexBuffer<uint32_t>           iboAll;
    std::list<Block>                         blocks;
  };
