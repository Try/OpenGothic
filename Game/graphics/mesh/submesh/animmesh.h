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
    AnimMesh(const ZenLoad::PackedSkeletalMesh& data);

    struct SubMesh {
      Material                       material;
      Tempest::IndexBuffer<uint32_t> ibo;
      std::string                    texName;
      };

    Tempest::VertexBuffer<VertexA> vbo;
    std::vector<SubMesh>           sub;
    Bounds                         bbox;
    const size_t                   bonesCount = 0;
  };
