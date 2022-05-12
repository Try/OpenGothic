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

    Tempest::AccelerationStructure blasSolid;

  private:
    using Item = ObjectsBucket::Item;

    struct Block {
      size_t                         iboOffset = 0;
      size_t                         iboLength = 0;
      Item                           mesh;
      Tempest::AccelerationStructure blas;
      };

    Tempest::VertexBuffer<Resources::Vertex> vbo;
    Tempest::IndexBuffer<uint32_t>           ibo;
    Tempest::IndexBuffer<uint32_t>           iboSolid;
    std::list<Block>                         blocks;
  };
