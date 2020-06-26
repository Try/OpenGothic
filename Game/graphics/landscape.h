#pragma once

#include <Tempest/Device>
#include <Tempest/Matrix4x4>
#include <Tempest/UniformBuffer>

#include <zenload/zTypes.h>

#include "bounds.h"
#include "material.h"
#include "meshobjects.h"
#include "resources.h"

class World;
class SceneGlobals;
class Light;
class PackedMesh;
class WorldView;

class Landscape final {
  public:
    Landscape(WorldView& owner, const SceneGlobals& scene, const PackedMesh& wmesh);

  private:
    struct Block {
      Tempest::IndexBuffer<uint32_t> ibo;
      MeshObjects::Mesh              mesh;
      };

    WorldView&                               owner;
    const SceneGlobals&                      scene;

    Tempest::VertexBuffer<Resources::Vertex> vbo;
    std::list<Block>                         blocks;
  };
