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

  private:
    using Item = ObjectsBucket::Item;

    struct Block {
      Tempest::IndexBuffer<uint32_t> ibo;
      Item                           mesh;
      };

    Tempest::VertexBuffer<Resources::Vertex> vbo;
    std::list<Block>                         blocks;
  };
