#pragma once

#include <Tempest/VertexBuffer>
#include <Tempest/IndexBuffer>
#include <Tempest/Device>
#include <Tempest/Matrix4x4>

#include "resources.h"
#include "staticmesh.h"

class AnimMesh {
  public:
    using Vertex=Resources::Vertex;
    AnimMesh(const ZenLoad::PackedMesh& data);
    AnimMesh(const ZenLoad::PackedSkeletalMesh& data);

    struct SubMesh {
      Tempest::Texture2d*            texture=nullptr;
      Tempest::IndexBuffer<uint32_t> ibo;
      };

    Tempest::VertexBuffer<Vertex>  vbo;
    std::vector<SubMesh>           sub;
  };
