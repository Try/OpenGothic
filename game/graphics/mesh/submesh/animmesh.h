#pragma once

#include <Tempest/VertexBuffer>
#include <Tempest/IndexBuffer>
#include <Tempest/Device>
#include <Tempest/Matrix4x4>

#include "resources.h"
#include "staticmesh.h"

class AnimMesh {
  public:
    using VertexA=Resources::VertexA;

    AnimMesh(const PackedMesh& data);

    struct SubMesh {
      Material                       material;
      std::string                    texName;
      size_t                         iboOffset = 0;
      size_t                         iboSize   = 0;
      };

    Tempest::VertexBuffer<VertexA> vbo;
    Tempest::IndexBuffer<uint32_t> ibo;
    Tempest::StorageBuffer         ibo8;
    std::vector<SubMesh>           sub;
    Bounds                         bbox;
    const size_t                   bonesCount = 0;
  };
